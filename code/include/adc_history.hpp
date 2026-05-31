// Purpose: Declares portable ADC history-ring views and helpers shared by the
// hardware sampler, acquisition engine, and host tests.
// Interface: Snapshot structs describe a stable window of interleaved CH1/CH2
// words; helpers map absolute pair sequences into contiguous borrowed spans.
// Constraints: History data remains sampler-owned, interleaved as CH1 then CH2,
// and callers must tolerate wrap by processing up to two spans.
// Ownership: This header owns no storage; all pointers are borrowed views.

#pragma once

#include <cstdint>

namespace picoscope {

constexpr std::uint16_t kAdcValueMask = 0x0FFFu;
constexpr std::uint16_t kAdcErrorMask = 0x8000u;

// Borrowed view of interleaved ADC words.
struct AdcSampleBlock {
    const std::uint16_t *words = nullptr;
    std::uint32_t word_count = 0;
    bool overflowed = false;
};

// Snapshot of the sampler-owned circular history ring at one instant.
struct AdcHistorySnapshot {
    const std::uint16_t *ring_words = nullptr;
    std::uint32_t ring_word_count = 0;
    std::uint32_t ring_pair_count = 0;
    std::uint32_t guard_pairs = 0;
    std::uint64_t oldest_available_pair_sequence = 0;
    std::uint64_t latest_complete_pair_sequence = 0;
    bool has_complete_pair = false;
    bool overflowed = false;
};

// Up to two contiguous spans covering a requested history range.
struct AdcHistoryRange {
    AdcSampleBlock spans[2] = {};
    std::uint8_t span_count = 0;
    std::uint32_t word_count = 0;
};

// Takes one ADC FIFO word, checks its conversion-error bit, and returns true
// when the low 12 bits can be used as a sample count.
constexpr bool adc_word_valid(std::uint16_t word)
{
    return (word & kAdcErrorMask) == 0u;
}

// Takes one ADC FIFO word, strips FIFO metadata bits, and returns the 12-bit
// sample count.
constexpr std::uint16_t adc_word_value(std::uint16_t word)
{
    return static_cast<std::uint16_t>(word & kAdcValueMask);
}

// Takes a snapshot and pair sequence, checks whether that pair is readable in
// the stable history window, and returns the result.
constexpr bool history_contains_pair(const AdcHistorySnapshot &snapshot,
                                     std::uint64_t pair_sequence)
{
    return snapshot.has_complete_pair &&
           pair_sequence >= snapshot.oldest_available_pair_sequence &&
           pair_sequence <= snapshot.latest_complete_pair_sequence;
}

constexpr bool power_of_two(std::uint32_t value)
{
    return value != 0u && (value & (value - 1u)) == 0u;
}

constexpr std::uint32_t history_pair_index(const AdcHistorySnapshot &snapshot,
                                           std::uint64_t pair_sequence)
{
    if (snapshot.ring_pair_count == 0u) {
        return 0u;
    }
    return power_of_two(snapshot.ring_pair_count)
               ? static_cast<std::uint32_t>(
                     pair_sequence & static_cast<std::uint64_t>(snapshot.ring_pair_count - 1u))
               : static_cast<std::uint32_t>(pair_sequence % snapshot.ring_pair_count);
}

// Takes a snapshot, absolute pair sequence, and output pair array; copies the
// CH1/CH2 words for that pair when present and returns success.
inline bool read_history_pair(const AdcHistorySnapshot &snapshot,
                              std::uint64_t pair_sequence,
                              std::uint16_t words[2])
{
    if (words == nullptr || snapshot.ring_words == nullptr ||
        snapshot.ring_pair_count == 0u ||
        !history_contains_pair(snapshot, pair_sequence)) {
        return false;
    }

    const std::uint32_t pair_index = history_pair_index(snapshot, pair_sequence);
    const std::uint32_t word_index = pair_index * 2u;
    words[0] = snapshot.ring_words[word_index];
    words[1] = snapshot.ring_words[word_index + 1u];
    return true;
}

// Takes a snapshot plus absolute pair range, splits it at the physical ring
// boundary when needed, and returns borrowed spans suitable for acquisition.
inline bool make_history_range(const AdcHistorySnapshot &snapshot,
                               std::uint64_t start_pair_sequence,
                               std::uint32_t pair_count,
                               AdcHistoryRange &range)
{
    range = {};
    if (snapshot.ring_words == nullptr || snapshot.ring_pair_count == 0u ||
        snapshot.ring_word_count != snapshot.ring_pair_count * 2u ||
        pair_count == 0u) {
        return false;
    }

    const std::uint64_t end_pair_sequence =
        start_pair_sequence + static_cast<std::uint64_t>(pair_count) - 1u;
    if (!history_contains_pair(snapshot, start_pair_sequence) ||
        !history_contains_pair(snapshot, end_pair_sequence)) {
        return false;
    }

    const std::uint32_t start_pair_index =
        history_pair_index(snapshot, start_pair_sequence);
    const std::uint32_t start_word_index = start_pair_index * 2u;
    const std::uint32_t requested_words = pair_count * 2u;
    const std::uint32_t first_words =
        requested_words < snapshot.ring_word_count - start_word_index
            ? requested_words
            : snapshot.ring_word_count - start_word_index;

    range.spans[0].words = snapshot.ring_words + start_word_index;
    range.spans[0].word_count = first_words;
    range.span_count = 1;
    range.word_count = requested_words;

    if (first_words < requested_words) {
        range.spans[1].words = snapshot.ring_words;
        range.spans[1].word_count = requested_words - first_words;
        range.span_count = 2;
    }
    return true;
}

} // namespace picoscope
