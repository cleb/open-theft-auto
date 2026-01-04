# Sprite Animation File Format

This document describes the JSON format used for defining animated sprites.

## File Structure

Sprite animation files are JSON files stored alongside their texture files in `assets/textures/`. 
The convention is to name them `<sprite-name>.json` to match the texture file `<sprite-name>.png`.

## Schema

```json
{
  "texture": "filename.png",
  "frameWidth": <number>,
  "frameHeight": <number>,
  "animations": {
    "<animation-name>": {
      "frames": [
        { "x": <number>, "y": <number> },
        ...
      ],
      "frameDuration": <number>,
      "loop": <boolean>
    }
  }
}
```

## Field Descriptions

### Root Object

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `texture` | string | Yes | Filename of the texture (relative to assets/textures/) |
| `frameWidth` | number | Yes | Width of each frame in pixels |
| `frameHeight` | number | Yes | Height of each frame in pixels |
| `animations` | object | Yes | Map of animation names to animation definitions |

### Animation Object

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `frames` | array | Yes | Array of frame coordinates |
| `frameDuration` | number | Yes | Duration of each frame in seconds |
| `loop` | boolean | No | Whether the animation loops (default: true) |

### Frame Object

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `x` | number | Yes | X coordinate of the top-left corner of the frame |
| `y` | number | Yes | Y coordinate of the top-left corner of the frame |

## Example

```json
{
  "texture": "player-animation.png",
  "frameWidth": 221,
  "frameHeight": 350,
  "animations": {
    "walk": {
      "frames": [
        { "x": 80, "y": 116 },
        { "x": 360, "y": 116 },
        { "x": 647, "y": 116 },
        { "x": 914, "y": 116 },
        { "x": 1199, "y": 116 },
        { "x": 80, "y": 532 },
        { "x": 360, "y": 532 },
        { "x": 647, "y": 532 },
        { "x": 914, "y": 532 },
        { "x": 1199, "y": 532 }
      ],
      "frameDuration": 0.1,
      "loop": true
    },
    "idle": {
      "frames": [
        { "x": 80, "y": 116 }
      ],
      "frameDuration": 1.0,
      "loop": true
    }
  }
}
```

## Multiple Animations

A single sprite sheet can contain multiple animations. Each animation is defined by name:

```json
{
  "texture": "character.png",
  "frameWidth": 64,
  "frameHeight": 64,
  "animations": {
    "walk_down": { ... },
    "walk_up": { ... },
    "walk_left": { ... },
    "walk_right": { ... },
    "attack": { ... },
    "idle": { ... }
  }
}
```

## Notes

- Coordinates are measured from the top-left of the texture image
- Frame indices are 0-based when accessed in code
- `frameDuration` is in seconds (0.1 = 10 FPS, 0.0167 = 60 FPS)
- All frames in an animation should have the same dimensions
