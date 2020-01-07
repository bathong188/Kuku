// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#include "kuku/kuku.h"

using namespace std;

namespace kuku
{
    QueryResult KukuTable::query(item_type item) const
    {
        if (is_empty_item(item))
        {
            throw invalid_argument("cannot query the empty item");
        }

        // Search the hash table
        auto lfc = loc_func_count();
        for (size_t i = 0; i < lfc; i++)
        {
            auto loc = location(item, i);
            if (are_equal_item(table_[loc], item))
            {
                return { loc, i };
            }
        }

        // Search the stash
        for (location_type loc = 0; loc < stash_.size(); loc++)
        {
            if (are_equal_item(stash_[loc], item))
            {
                return { loc, ~size_t(0) };
            }
        }

        // Not found
        return { 0, max_loc_func_count };
    }

    KukuTable::KukuTable(
        int table_size, table_size_type stash_size,
        size_t loc_func_count, item_type loc_func_seed,
        uint64_t max_probe, item_type empty_item) :
        table_size_(table_size),
        stash_size_(stash_size),
        loc_func_seed_(loc_func_seed),
        max_probe_(max_probe),
        empty_item_(empty_item),
        last_insert_fail_item_(empty_item_)
    {
        if (!loc_func_count || loc_func_count > max_loc_func_count)
        {
            throw invalid_argument("invalid loc_func_count");
        }
        if (table_size <=1 || ceil(log2(table_size)) > max_log_table_size)
        {
            throw invalid_argument("invalid table_size");
        }
        if (!max_probe)
        {
            throw invalid_argument("max_probe cannot be zero");
        }

        // Allocate the hash table
        table_.resize(table_size_, empty_item_);

        // Create the location (hash) functions
        generate_loc_funcs(loc_func_count, loc_func_seed_);

        gen_ = std::mt19937_64(rd_());

        u_ = std::uniform_int_distribution<size_t>(0, loc_func_count-1);
    }

    set<location_type> KukuTable::all_locations(item_type item) const
    {
        set<location_type> result;
        for (auto lf : loc_funcs_)
        {
            result.emplace(lf(item));
        }
        return result;
    }

    void KukuTable::clear_table()
    {
        size_t sz = table_.size();
        table_.resize(0);
        table_.resize(sz, empty_item_);
        stash_.clear();
        last_insert_fail_item_ = empty_item_;
    }

    void KukuTable::generate_loc_funcs(size_t loc_func_count, item_type seed)
    {
        loc_funcs_.clear();
        while (loc_func_count--)
        {
            loc_funcs_.emplace_back(table_size(), seed);
            increment_item(seed);
        }
    }

    bool KukuTable::insert(item_type item, uint64_t level)
    {
        if (is_empty_item(item))
        {
            throw invalid_argument("cannot insert the null item");
        }
        if (level >= max_probe_)
        {
            if (stash_.size() < stash_size_)
            {
                stash_.push_back(item);
                inserted_items_++;
                return true;
            }
            else
            {
                last_insert_fail_item_ = item;
                return false;
            }
        }

        // for loop over all possible locations
        //vector<location_type> locs(loc_func_count());
        location_type loc;
        for (int i = 0; i < loc_func_count(); i++) {
            loc = loc_funcs_[i](item);
            if (is_empty_item(table_[loc])) {
                table_[loc] = item; 
                inserted_items_++;
                return true;
            }
        }
        // pop out a random item and recursively insert.
        
        size_t loc_index = u_(gen_);
        loc = loc_funcs_[loc_index](item);
        
        auto old_item = swap(item, loc);

        return insert(old_item, level + 1);
    }
}
