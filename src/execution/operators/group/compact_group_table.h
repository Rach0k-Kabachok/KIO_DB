#pragma once

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace exec_group {

class CompactGroupTable {
public:
    CompactGroupTable(size_t key_size, size_t reserve_groups);

    template <typename EqualsExisting>
    std::pair<size_t, bool> FindOrInsert(
            const std::vector<uint64_t>& key,
            EqualsExisting equals_existing) {
        if (ShouldGrow()) {
            Rehash(slots_.empty() ? kInitialCapacity : slots_.size() * 2);
        }

        const size_t slot_idx = FindSlot(key.data(), equals_existing);
        Slot& slot = slots_[slot_idx];
        if (slot.occupied) {
            return {slot.group_idx, false};
        }

        const size_t group_idx = group_count_++;
        keys_.insert(keys_.end(), key.begin(), key.end());
        slot.group_idx = group_idx;
        slot.occupied = true;
        return {group_idx, true};
    }

    size_t Size() const;

private:
    struct Slot {
        size_t group_idx = 0;
        bool occupied = false;
    };

    static constexpr size_t kInitialCapacity = 8;

    static size_t CapacityFor(size_t expected_groups);
    static size_t HashEncodedKey(const uint64_t* key, size_t key_size);
    static bool EncodedKeysEqual(const uint64_t* lhs,
                                 const uint64_t* rhs,
                                 size_t key_size);

    void Reserve(size_t expected_groups);
    bool ShouldGrow() const;
    const uint64_t* StoredKey(size_t group_idx) const;

    template <typename EqualsExisting>
    size_t FindSlot(const uint64_t* key,
                    EqualsExisting equals_existing) const {
        size_t slot_idx = HashEncodedKey(key, key_size_) & (slots_.size() - 1);
        while (true) {
            const Slot& slot = slots_[slot_idx];
            if (!slot.occupied) {
                return slot_idx;
            }
            if (EncodedKeysEqual(StoredKey(slot.group_idx), key, key_size_) &&
                equals_existing(slot.group_idx)) {
                return slot_idx;
            }
            slot_idx = (slot_idx + 1) & (slots_.size() - 1);
        }
    }

    size_t FindEmptySlot(const uint64_t* key) const;
    void Rehash(size_t new_capacity);

    size_t key_size_;
    size_t group_count_ = 0;
    std::vector<Slot> slots_;
    std::vector<uint64_t> keys_;
};

}  // namespace exec_group
