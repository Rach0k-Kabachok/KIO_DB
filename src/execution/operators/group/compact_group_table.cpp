#include "execution/operators/group/compact_group_table.h"

#include <functional>
#include <utility>

#include "global/scalar_value.h"

namespace exec_group {

CompactGroupTable::CompactGroupTable(size_t key_size, size_t reserve_groups)
    : key_size_(key_size) {
    Reserve(reserve_groups);
}

size_t CompactGroupTable::Size() const {
    return group_count_;
}

size_t CompactGroupTable::CapacityFor(size_t expected_groups) {
    size_t capacity = kInitialCapacity;
    while (capacity * 3 < expected_groups * 4) {
        capacity *= 2;
    }
    return capacity;
}

size_t CompactGroupTable::HashEncodedKey(const uint64_t* key,
                                         size_t key_size) {
    size_t result = 0;
    for (size_t idx = 0; idx < key_size; idx++) {
        result = scalar::HashCombine(result, std::hash<uint64_t>()(key[idx]));
    }
    return result;
}

bool CompactGroupTable::EncodedKeysEqual(const uint64_t* lhs,
                                         const uint64_t* rhs,
                                         size_t key_size) {
    for (size_t idx = 0; idx < key_size; idx++) {
        if (lhs[idx] != rhs[idx]) {
            return false;
        }
    }
    return true;
}

void CompactGroupTable::Reserve(size_t expected_groups) {
    const size_t capacity = CapacityFor(expected_groups);
    if (capacity > slots_.size()) {
        Rehash(capacity);
    }
    keys_.reserve(expected_groups * key_size_);
}

bool CompactGroupTable::ShouldGrow() const {
    return slots_.empty() ||
           (group_count_ + 1) * 4 >= slots_.size() * 3;
}

const uint64_t* CompactGroupTable::StoredKey(size_t group_idx) const {
    return keys_.data() + group_idx * key_size_;
}

size_t CompactGroupTable::FindEmptySlot(const uint64_t* key) const {
    size_t slot_idx = HashEncodedKey(key, key_size_) & (slots_.size() - 1);
    while (slots_[slot_idx].occupied) {
        slot_idx = (slot_idx + 1) & (slots_.size() - 1);
    }
    return slot_idx;
}

void CompactGroupTable::Rehash(size_t new_capacity) {
    std::vector<Slot> old_slots = std::move(slots_);
    slots_.clear();
    slots_.resize(new_capacity);

    for (const Slot& slot : old_slots) {
        if (!slot.occupied) {
            continue;
        }
        slots_[FindEmptySlot(StoredKey(slot.group_idx))] = slot;
    }
}

}  // namespace exec_group
