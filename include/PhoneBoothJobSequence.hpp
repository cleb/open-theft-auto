#pragma once

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

class PhoneBoothJobSequence {
public:
    void setJobs(std::vector<std::string> jobIds) {
        m_jobIds = std::move(jobIds);
        m_currentIndex = 0;
    }

    const std::string* currentJobId() const {
        return m_currentIndex < m_jobIds.size() ? &m_jobIds[m_currentIndex] : nullptr;
    }

    bool advance() {
        if (!currentJobId()) {
            return false;
        }
        ++m_currentIndex;
        return currentJobId() != nullptr;
    }

private:
    std::vector<std::string> m_jobIds;
    std::size_t m_currentIndex = 0;
};
