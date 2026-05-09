#!/usr/bin/env python3
import argparse
from collections import deque
from pathlib import Path

import torch
from diffusers import EulerDiscreteScheduler, StableDiffusionXLPipeline
from PIL import ImageFilter


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Generate an image with the local DreamShaper XL Lightning model."
    )
    parser.add_argument("prompt", help="Text prompt to render")
    parser.add_argument(
        "--negative",
        default="low quality, blurry, distorted, deformed, watermark, text",
        help="Negative prompt",
    )
    parser.add_argument("--width", type=int, default=1024, help="Image width")
    parser.add_argument("--height", type=int, default=1024, help="Image height")
    parser.add_argument("--steps", type=int, default=6, help="Inference steps")
    parser.add_argument("--guidance", type=float, default=1.5, help="CFG guidance scale")
    parser.add_argument("--seed", type=int, default=None, help="Random seed")
    parser.add_argument(
        "--model",
        default="models/DreamShaperXL_Lightning.safetensors",
        help="Path to the .safetensors checkpoint",
    )
    parser.add_argument(
        "--output",
        default=None,
        help="Output PNG path. Defaults to outputs/generated-<seed>.png",
    )
    parser.add_argument(
        "--transparent-background",
        action="store_true",
        help=(
            "Remove the edge-connected light background and save RGBA output. "
            "For best results, prompt for an isolated subject on a pure white background."
        ),
    )
    parser.add_argument(
        "--background-threshold",
        type=int,
        default=218,
        help="Minimum RGB value treated as removable background when using --transparent-background.",
    )
    parser.add_argument(
        "--background-tolerance",
        type=int,
        default=34,
        help="Maximum RGB channel spread treated as neutral background.",
    )
    parser.add_argument(
        "--alpha-blur",
        type=float,
        default=0.6,
        help="Gaussian blur radius for the generated alpha edge.",
    )
    return parser.parse_args()


def remove_light_edge_background(
    image,
    threshold: int,
    tolerance: int,
    alpha_blur: float,
):
    image = image.convert("RGBA")
    width, height = image.size
    pixels = image.load()
    seen = bytearray(width * height)
    queue = deque()

    def index(x: int, y: int) -> int:
        return y * width + x

    def is_background(x: int, y: int) -> bool:
        r, g, b, a = pixels[x, y]
        return (
            a > 0
            and r >= threshold
            and g >= threshold
            and b >= threshold
            and max(r, g, b) - min(r, g, b) <= tolerance
        )

    for x in range(width):
        for y in (0, height - 1):
            if is_background(x, y) and not seen[index(x, y)]:
                seen[index(x, y)] = 1
                queue.append((x, y))
    for y in range(height):
        for x in (0, width - 1):
            if is_background(x, y) and not seen[index(x, y)]:
                seen[index(x, y)] = 1
                queue.append((x, y))

    while queue:
        x, y = queue.popleft()
        for nx, ny in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
            if (
                0 <= nx < width
                and 0 <= ny < height
                and not seen[index(nx, ny)]
                and is_background(nx, ny)
            ):
                seen[index(nx, ny)] = 1
                queue.append((nx, ny))

    alpha = image.getchannel("A")
    alpha_pixels = alpha.load()
    for y in range(height):
        for x in range(width):
            if seen[index(x, y)]:
                alpha_pixels[x, y] = 0

    if alpha_blur > 0:
        alpha = alpha.filter(ImageFilter.GaussianBlur(radius=alpha_blur))
    image.putalpha(alpha)
    return image


def main() -> None:
    args = parse_args()
    root = Path(__file__).resolve().parent
    model_path = (root / args.model).resolve()
    if not model_path.exists():
        raise FileNotFoundError(f"Model checkpoint not found: {model_path}")

    if args.seed is None:
        args.seed = torch.seed() % (2**32)

    if torch.cuda.is_available():
        device = "cuda"
        dtype = torch.float16
        generator_device = "cuda"
    elif torch.backends.mps.is_available():
        device = "mps"
        dtype = torch.float16
        generator_device = "cpu"
    else:
        device = "cpu"
        dtype = torch.float32
        generator_device = "cpu"

    generator = torch.Generator(device=generator_device).manual_seed(args.seed)

    pipe = StableDiffusionXLPipeline.from_single_file(
        str(model_path),
        torch_dtype=dtype,
        use_safetensors=True,
    )
    pipe.scheduler = EulerDiscreteScheduler.from_config(
        pipe.scheduler.config,
        timestep_spacing="trailing",
    )
    if device == "cuda":
        pipe.enable_model_cpu_offload()
    else:
        pipe.to(device)

    if hasattr(pipe.vae, "enable_tiling"):
        pipe.vae.enable_tiling()
    if hasattr(pipe.vae, "enable_slicing"):
        pipe.vae.enable_slicing()

    image = pipe(
        prompt=args.prompt,
        negative_prompt=args.negative,
        width=args.width,
        height=args.height,
        num_inference_steps=args.steps,
        guidance_scale=args.guidance,
        generator=generator,
    ).images[0]

    output = Path(args.output) if args.output else root / "outputs" / f"generated-{args.seed}.png"
    if args.transparent_background and output.suffix.lower() != ".png":
        raise ValueError("--transparent-background requires a .png output path to preserve alpha.")

    if args.transparent_background:
        image = remove_light_edge_background(
            image,
            threshold=args.background_threshold,
            tolerance=args.background_tolerance,
            alpha_blur=args.alpha_blur,
        )

    output.parent.mkdir(parents=True, exist_ok=True)
    image.save(output)
    print(output)


if __name__ == "__main__":
    main()
