/*
 * Copyright 2016-2017 Internet Corporation for Assigned Names and Numbers.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, you can obtain one at https://mozilla.org/MPL/2.0/.
 */

/*
 * Developed by Sinodun IT (www.sinodun.com)
 */

#include <cstddef>
#include <stdexcept>

#include "blockcbor.hpp"
#include "blockcbordata.hpp"

namespace block_cbor {
    /**
     * \brief Fixed file format string.
     */
    const std::string& FILE_FORMAT_ID = "C-DNS";

    /**
     * \brief Current output format major version.
     */
    const unsigned FILE_FORMAT_MAJOR_VERSION = 0;

    /**
     * \brief Current output format minor version.
     */
    const unsigned FILE_FORMAT_MINOR_VERSION = 5;

    /**
     * \brief Pre-draft format id.
     */
    const std::string& OLD_FILE_FORMAT_ID = "DNS-STAT";

    /**
     * \brief Pre-draft version ID.
     */
    const unsigned OLD_FILE_FORMAT_VERSION = 2;

    /**
     ** Old formats tables.
     **/

    /**
     * \brief 0.2 statistics.
     */
    const std::vector<BlockStatisticsField> version_0_2_block_statistics = {
        BlockStatisticsField::total_packets,
        BlockStatisticsField::total_pairs,
        BlockStatisticsField::unmatched_queries,
        BlockStatisticsField::unmatched_responses,
        BlockStatisticsField::completely_malformed_packets,
        BlockStatisticsField::compactor_non_dns_packets,
        BlockStatisticsField::compactor_out_of_order_packets,
        BlockStatisticsField::compactor_missing_pairs,
        BlockStatisticsField::compactor_missing_packets,
        BlockStatisticsField::compactor_missing_non_dns,
    };

    /**
     * \brief 0.2 query/response.
     */
    const std::vector<QueryResponseField> version_0_2_query_response = {
        QueryResponseField::time_useconds,
        QueryResponseField::client_address_index,
        QueryResponseField::client_port,
        QueryResponseField::transaction_id,
        QueryResponseField::query_signature_index,
        QueryResponseField::client_hoplimit,
        QueryResponseField::delay_useconds,
        QueryResponseField::query_name_index,
        QueryResponseField::response_size,
        QueryResponseField::query_extended,
        QueryResponseField::response_extended,
        QueryResponseField::query_size,
    };

    /**
     * \brief get the number of elements in a C array.
     */
    template<typename T, std::size_t N>
    constexpr std::size_t countof(const T (&)[N]) noexcept
    {
        return N;
    }

    FilePreambleField file_preamble_field(unsigned index, bool old)
    {
        std::size_t arrsize;
        const FilePreambleField* arr;

        if ( old )
        {
            arr = old_file_preamble;
            arrsize = countof(old_file_preamble);
        }
        else
        {
            arr = current_file_preamble;
            arrsize = countof(current_file_preamble);
        }

        if ( index >= arrsize )
            return FilePreambleField::unknown;
        else
            return arr[index];
    }

    FileVersionFields::FileVersionFields()
        : configuration_(current_configuration, current_configuration + countof(current_configuration)),
          block_(current_block, current_block + countof(current_block)),
          block_preamble_(current_block_preamble, current_block_preamble + countof(current_block_preamble)),
          block_statistics_(current_block_statistics, current_block_statistics + countof(current_block_statistics)),
          block_tables_(current_block_tables, current_block_tables + countof(current_block_tables)),
          query_response_(current_query_response, current_query_response + countof(current_query_response)),
          class_type_(current_class_type, current_class_type + countof(current_class_type)),
          query_signature_(current_query_signature, current_query_signature + countof(current_query_signature)),
          question_(current_question, current_question + countof(current_question)),
          rr_(current_rr, current_rr + countof(current_rr)),
          query_response_extended_(current_query_response_extended, current_query_response_extended + countof(current_query_response_extended)),
          address_event_count_(current_address_event_count, current_address_event_count + countof(current_address_event_count))
    {
    }

    FileVersionFields::FileVersionFields(unsigned major_version,
                                         unsigned minor_version,
                                         unsigned private_version)
        : FileVersionFields()
    {
        // If the current version, we're done.
        if ( major_version == FILE_FORMAT_MAJOR_VERSION &&
             minor_version == FILE_FORMAT_MINOR_VERSION )
            return;

        if ( major_version == 0 && minor_version == OLD_FILE_FORMAT_VERSION )
        {
            block_statistics_ = version_0_2_block_statistics;
            query_response_ = version_0_2_query_response;
            return;
        }

        throw cbor_file_format_error("Unknown file format version");
    }

    ConfigurationField FileVersionFields::configuration_field(unsigned index) const
    {
        if ( index < configuration_.size() )
            return configuration_[index];
        else
            return ConfigurationField::unknown;
    }

    BlockField FileVersionFields::block_field(unsigned index) const
    {
        if ( index < block_.size() )
            return block_[index];
        else
            return BlockField::unknown;
    }

    BlockPreambleField FileVersionFields::block_preamble_field(unsigned index) const
    {
        if ( index < block_preamble_.size() )
            return block_preamble_[index];
        else
            return BlockPreambleField::unknown;
    }

    BlockStatisticsField FileVersionFields::block_statistics_field(unsigned index) const
    {
        if ( index < block_statistics_.size() )
            return block_statistics_[index];
        else
            return BlockStatisticsField::unknown;
    }

    BlockTablesField FileVersionFields::block_tables_field(unsigned index) const
    {
        if ( index < block_tables_.size() )
            return block_tables_[index];
        else
            return BlockTablesField::unknown;
    }

    QueryResponseField FileVersionFields::query_response_field(unsigned index) const
    {
        if ( index < query_response_.size() )
            return query_response_[index];
        else
            return QueryResponseField::unknown;
    }

    ClassTypeField FileVersionFields::class_type_field(unsigned index) const
    {
        if ( index < class_type_.size() )
            return class_type_[index];
        else
            return ClassTypeField::unknown;
    }

    QuerySignatureField FileVersionFields::query_signature_field(unsigned index) const
    {
        if ( index < query_signature_.size() )
            return query_signature_[index];
        else
            return QuerySignatureField::unknown;
    }

    QuestionField FileVersionFields::question_field(unsigned index) const
    {
        if ( index < question_.size() )
            return question_[index];
        else
            return QuestionField::unknown;
    }

    RRField FileVersionFields::rr_field(unsigned index) const
    {
        if ( index < rr_.size() )
            return rr_[index];
        else
            return RRField::unknown;
    }

    QueryResponseExtendedField FileVersionFields::query_response_extended_field(unsigned index) const
    {
        if ( index < query_response_extended_.size() )
            return query_response_extended_[index];
        else
            return QueryResponseExtendedField::unknown;
    }

    AddressEventCountField FileVersionFields::address_event_count_field(unsigned index) const
    {
        if ( index < address_event_count_.size() )
            return address_event_count_[index];
        else
            return AddressEventCountField::unknown;
    }
};
