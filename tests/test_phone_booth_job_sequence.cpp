#include "PhoneBoothJobSequence.hpp"

#include <cassert>
#include <string>
#include <vector>

int main() {
    PhoneBoothJobSequence sequence;
    sequence.setJobs({"custom_first", "custom_second", "custom_third"});

    assert(sequence.currentJobId() && *sequence.currentJobId() == "custom_first");
    assert(sequence.advance());
    assert(sequence.currentJobId() && *sequence.currentJobId() == "custom_second");
    assert(sequence.advance());
    assert(sequence.currentJobId() && *sequence.currentJobId() == "custom_third");
    assert(!sequence.advance());
    assert(sequence.currentJobId() == nullptr);
    assert(!sequence.advance());

    sequence.setJobs({"replacement"});
    assert(sequence.currentJobId() && *sequence.currentJobId() == "replacement");

    sequence.setJobs(std::vector<std::string>{});
    assert(sequence.currentJobId() == nullptr);

    return 0;
}
