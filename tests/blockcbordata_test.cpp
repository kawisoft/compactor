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

#include <chrono>
#include <memory>
#include <unordered_map>
#include <vector>
#include <utility>

#include <boost/functional/hash.hpp>

#include "catch.hpp"

#include "baseoutputwriter.hpp"
#include "cborencoder.hpp"
#include "dnsmessage.hpp"
#include "queryresponse.hpp"
#include "makeunique.hpp"

#include "blockcbordata.hpp"

using namespace block_cbor;

namespace {
    class TestCborEncoder : public CborBaseEncoder
    {
    public:
        TestCborEncoder() : CborBaseEncoder() {}

        void clear()
        {
            bytes.clear();
        }

        bool compareBytes(const uint8_t *buf, std::size_t buflen)
        {
            if ( buflen != bytes.size() )
                return false;

            for ( auto b : bytes )
                if ( b != *buf++ )
                    return false;

            return true;
        }

    protected:
        virtual void writeBytes(const uint8_t *p, std::ptrdiff_t nBytes)
        {
            while ( nBytes-- > 0 )
                bytes.push_back(*p++);
        }

    private:
        std::vector<uint8_t> bytes;
    };

    class TestCborDecoder : public CborBaseDecoder
    {
    public:
        TestCborDecoder()
            : CborBaseDecoder() {}

        TestCborDecoder(const std::vector<uint8_t>& bytes)
            : CborBaseDecoder(), bytes_(bytes) {}

        void set_bytes(const std::vector<uint8_t>& bytes)
        {
            bytes_ = bytes;
        }

    protected:
        virtual unsigned readBytes(uint8_t* p, std::ptrdiff_t n_bytes)
        {
            if ( bytes_.empty() )
                throw cbor_end_of_input();

            // Return input a bit at a time to test the refill logic.
            int res = bytes_.size() / 2;
            if ( res == 0 && !bytes_.empty() )
                res = 1;
            if ( res > n_bytes )
                res = n_bytes;
            for ( int i = 0; i < res; ++i )
                *p++ = bytes_[i];
            bytes_.erase(bytes_.begin(), bytes_.begin() + res);
            return res;
        }

    private:
        std::vector<uint8_t> bytes_;
    };

    // In reality, we'd use the plain int as the key value.
    // But here we implement a whole-item key for testing.
    struct IntItem
    {
        int val;

        const IntItem& key() const
        {
            return *this;
        }

        bool operator==(const IntItem& rhs) const
        {
            return ( val == rhs.val );
        }
        bool operator!=(const IntItem& rhs) const
        {
            return !( *this == rhs );
        }
        void readCbor(CborBaseDecoder& dec, const FileVersionFields&)
        {
            val = dec.read_unsigned();
        }
        void writeCbor(CborBaseEncoder& enc)
        {
            enc.write(val);
        }
    };

    std::size_t hash_value(const IntItem& i)
    {
        return boost::hash_value(i.val);
    }

}

SCENARIO("IndexVectorItems can be compared and written", "[block]")
{
    GIVEN("Some sample vectors")
    {
        std::vector<index_t> v1{1, 2, 3, 4};
        std::vector<index_t> v2{1, 2, 3, 4};
        std::vector<index_t> v3{1, 2, 3, 5};
        IndexVectorItem iv1, iv2, iv3;
        iv1.vec = v1;
        iv2.vec = v2;
        iv3.vec = v3;

        WHEN("idential items are compared")
        {
            THEN("they compare equal")
            {
                REQUIRE(iv1.vec == iv2.vec);
            }
        }

        WHEN("different items are compared")
        {
            THEN("they don't compare equal")
            {
                REQUIRE(iv1.vec != iv3.vec);
            }
        }

        WHEN("values are encoded")
        {
            TestCborEncoder tcbe;
            iv1.writeCbor(tcbe);
            iv3.writeCbor(tcbe);
            tcbe.flush();

            THEN("the encoding is as expected")
            {
                const uint8_t EXPECTED[] =
                    {
                        (4 << 5) | 31,
                        1, 2, 3, 4,
                        0xff,
                        (4 << 5) | 31,
                        1, 2, 3, 5,
                        0xff,
                    };

                REQUIRE(tcbe.compareBytes(EXPECTED, sizeof(EXPECTED)));
            }
        }
    }
}

SCENARIO("ByteStringItems can be compared and written", "[block]")
{
    GIVEN("Some sample strings")
    {
        ByteStringItem si1, si2, si3;
        si1.str = "Hello"_b;
        si2.str = "Hello"_b;
        si3.str = "World"_b;

        WHEN("idential items are compared")
        {
            THEN("they compare equal")
            {
                REQUIRE(si1.str == si2.str);
            }
        }

        WHEN("different items are compared")
        {
            THEN("they don't compare equal")
            {
                REQUIRE(si1.str != si3.str);
            }
        }

        WHEN("values are encoded")
        {
            TestCborEncoder tcbe;
            si1.writeCbor(tcbe);
            si3.writeCbor(tcbe);
            tcbe.flush();

            THEN("the encoding is as expected")
            {
                const uint8_t EXPECTED[] =
                    {
                        (2 << 5) | 5,
                        'H', 'e', 'l', 'l', 'o',
                        (2 << 5) | 5,
                        'W', 'o', 'r', 'l', 'd',
                    };

                REQUIRE(tcbe.compareBytes(EXPECTED, sizeof(EXPECTED)));
            }
        }
    }
}

SCENARIO("IPAddressItems can be compared and written", "[block]")
{
    GIVEN("Some sample IP addresses")
    {
        Tins::IPv4Address a4("193.0.29.226");
        Tins::IPv6Address a6("2001:67c:64:42:bdcd:34ce:2801:9686");
        IPAddressItem ai1, ai2, ai3;
        ai1.addr = IPAddress(a4);
        ai2.addr = IPAddress(a4);
        ai3.addr = IPAddress(a6);


        WHEN("idential items are compared")
        {
            THEN("they compare equal")
            {
                REQUIRE(ai1.addr == ai2.addr);
            }
        }

        WHEN("different items are compared")
        {
            THEN("they don't compare equal")
            {
                REQUIRE(ai1.addr != ai3.addr);
            }
        }

        WHEN("values are encoded")
        {
            TestCborEncoder tcbe;
            ai1.writeCbor(tcbe);
            ai3.writeCbor(tcbe);
            tcbe.flush();

            THEN("the encoding is as expected")
            {
                const uint8_t EXPECTED[] =
                    {
                        (2 << 5) | 4,
                        193, 0, 29, 226,
                        (2 << 5) | 16,
                        0x20, 0x01, 0x06, 0x7c, 0x00, 0x64, 0x00, 0x42,
                        0xbd, 0xcd, 0x34, 0xce, 0x28, 0x01, 0x96, 0x86,
                    };

                REQUIRE(tcbe.compareBytes(EXPECTED, sizeof(EXPECTED)));
            }
        }
    }
}

SCENARIO("ClassTypes can be compared and written", "[block]")
{
    GIVEN("Some sample class/type items")
    {
        ClassType ct1, ct2, ct3;
        ct1.qclass = ct2.qclass = ct3.qclass = CaptureDNS::INTERNET;
        ct1.qtype = ct2.qtype = CaptureDNS::A;
        ct3.qtype = CaptureDNS::CNAME;

        WHEN("idential items are compared")
        {
            THEN("they compare equal")
            {
                REQUIRE(ct1 == ct2);
            }
        }

        WHEN("different items are compared")
        {
            THEN("they don't compare equal")
            {
                REQUIRE(ct1 != ct3);
            }
        }

        WHEN("values are encoded")
        {
            TestCborEncoder tcbe;
            ct1.writeCbor(tcbe);
            ct3.writeCbor(tcbe);
            tcbe.flush();

            THEN("the encoding is as expected")
            {
                const uint8_t EXPECTED[] =
                    {
                        (5 << 5) | 2,
                        0, 1,
                        1, 1,
                        (5 << 5) | 2,
                        0, 5,
                        1, 1,
                    };

                REQUIRE(tcbe.compareBytes(EXPECTED, sizeof(EXPECTED)));
            }
        }
    }
}

SCENARIO("Questions can be compared and written", "[block]")
{
    GIVEN("Some sample question items")
    {
        Question q1, q2, q3;
        q1.qname = q2.qname = 1;
        q3.qname = 2;
        q1.classtype = q2.classtype = 20;
        q3.classtype = 19;

        WHEN("idential items are compared")
        {
            THEN("they compare equal")
            {
                REQUIRE(q1 == q2);
            }
        }

        WHEN("different items are compared")
        {
            THEN("they don't compare equal")
            {
                REQUIRE(q1 != q3);
            }
        }

        WHEN("values are encoded")
        {
            TestCborEncoder tcbe;
            q1.writeCbor(tcbe);
            q3.writeCbor(tcbe);
            tcbe.flush();

            THEN("the encoding is as expected")
            {
                const uint8_t EXPECTED[] =
                    {
                        (5 << 5) | 2,
                        0, 1,
                        1, 20,
                        (5 << 5) | 2,
                        0, 2,
                        1, 19,
                    };

                REQUIRE(tcbe.compareBytes(EXPECTED, sizeof(EXPECTED)));
            }
        }
    }
}

SCENARIO("ResourceRecords can be compared and written", "[block]")
{
    GIVEN("Some sample resource record items")
    {
        ResourceRecord rr1, rr2, rr3;
        rr1.name = rr2.name = rr3.name = 1;
        rr1.classtype = rr2.classtype = 12;
        rr3.classtype = 13;
        rr1.ttl = rr2.ttl = rr3.ttl = 10;
        rr1.rdata = rr2.rdata = rr3.rdata = 11;

        WHEN("idential items are compared")
        {
            THEN("they compare equal")
            {
                REQUIRE(rr1 == rr2);
            }
        }

        WHEN("different items are compared")
        {
            THEN("they don't compare equal")
            {
                REQUIRE(rr1 != rr3);
            }
        }

        WHEN("values are encoded")
        {
            TestCborEncoder tcbe;
            rr1.writeCbor(tcbe);
            rr3.writeCbor(tcbe);
            tcbe.flush();

            THEN("the encoding is as expected")
            {
                const uint8_t EXPECTED[] =
                    {
                        (5 << 5) | 4,
                        0, 1,
                        1, 12,
                        2, 10,
                        3, 11,
                        (5 << 5) | 4,
                        0, 1,
                        1, 13,
                        2, 10,
                        3, 11,
                    };

                REQUIRE(tcbe.compareBytes(EXPECTED, sizeof(EXPECTED)));
            }
        }
    }
}

SCENARIO("QuerySignatures can be compared and written", "[block]")
{
    GIVEN("Some sample QuerySignature items")
    {
        QuerySignature qs1, qs2;
        qs1.server_address = 1;
        qs1.server_port = 2;
        qs1.transport_flags = 3;
        qs1.qr_flags = 0x1f;
        qs1.qdcount = 1;
        qs1.query_rcode = 22;
        qs1.response_rcode = 23;
        qs1.query_opcode = 2;
        qs1.query_edns_version = 0;
        qs1.query_edns_payload_size = 22;
        qs1.query_opt_rdata = 4;
        qs1.dns_flags = 8;
        qs1.query_classtype = 3;
        qs1.query_ancount = 2;
        qs1.query_arcount = 3;
        qs1.query_nscount = 4;
        qs2 = qs1;

        WHEN("idential items are compared")
        {
            THEN("they compare equal")
            {
                REQUIRE(qs1 == qs2);
            }
        }

        WHEN("different items are compared")
        {
            qs2.query_opcode = 4;

            THEN("they don't compare equal")
            {
                REQUIRE(qs1 != qs2);
            }
        }

        WHEN("values are encoded")
        {
            TestCborEncoder tcbe;
            qs1.writeCbor(tcbe);
            tcbe.flush();

            THEN("the encoding is as expected")
            {
                constexpr uint8_t EXPECTED[] =
                    {
                        (5 << 5) | 31,
                        find_query_signature_index(QuerySignatureField::server_address_index), 1,
                        find_query_signature_index(QuerySignatureField::server_port), 2,
                        find_query_signature_index(QuerySignatureField::transport_flags), 3,
                        find_query_signature_index(QuerySignatureField::qr_dns_flags), 8,
                        find_query_signature_index(QuerySignatureField::qr_sig_flags), (0 << 5) | 24, 0x1f,
                        find_query_signature_index(QuerySignatureField::query_qd_count), 1,
                        find_query_signature_index(QuerySignatureField::query_classtype_index), 3,
                        find_query_signature_index(QuerySignatureField::query_rcode), 22,
                        find_query_signature_index(QuerySignatureField::query_opcode), 2,
                        find_query_signature_index(QuerySignatureField::query_an_count), 2,
                        find_query_signature_index(QuerySignatureField::query_ar_count), 3,
                        find_query_signature_index(QuerySignatureField::query_ns_count), 4,
                        find_query_signature_index(QuerySignatureField::edns_version), 0,
                        find_query_signature_index(QuerySignatureField::udp_buf_size), 22,
                        find_query_signature_index(QuerySignatureField::opt_rdata_index), 4,
                        find_query_signature_index(QuerySignatureField::response_rcode), 23,
                        0xff
                    };

                REQUIRE(tcbe.compareBytes(EXPECTED, sizeof(EXPECTED)));
            }
        }
    }
}

SCENARIO("AddressEventItems can be compared", "[block]")
{
    GIVEN("Some sample AddressEventItems")
    {
        AddressEventItem aei1, aei2;
        aei1.type = AddressEvent::EventType::TCP_RESET;
        aei1.code = 11;
        aei1.address = 99;
        aei2 = aei1;

        WHEN("idential items are compared")
        {
            THEN("they compare equal")
            {
                REQUIRE(aei1 == aei2);
            }
        }

        WHEN("different items are compared")
        {
            aei2.address = 4;

            THEN("they don't compare equal")
            {
                REQUIRE(aei1 != aei2);
            }
        }
    }
}

SCENARIO("AddressEventCounts can be written", "[block]")
{
    GIVEN("A sample AddressEventCount")
    {
        AddressEventCount aec;
        aec.aei.type = AddressEvent::EventType::TCP_RESET;
        aec.aei.code = 11;
        aec.aei.address = 10;
        aec.count = 22;

        WHEN("values are encoded")
        {
            TestCborEncoder tcbe;
            aec.writeCbor(tcbe);
            tcbe.flush();

            THEN("the encoding is as expected")
            {
                const uint8_t EXPECTED[] =
                    {
                        (5 << 5) | 31,
                        0, 0,
                        1, 11,
                        2, 10,
                        3, 22,
                        0xff,
                    };

                REQUIRE(tcbe.compareBytes(EXPECTED, sizeof(EXPECTED)));
            }
        }
    }
}

SCENARIO("QueryResponseItems can be written", "[block]")
{
    GIVEN("A sample QueryResponseItem item")
    {
        QueryResponseItem qri1;
        qri1.qr_flags = 0x1f;
        qri1.client_address = 1;
        qri1.client_port = 2;
        qri1.hoplimit = 20;
        qri1.id = 21;
        qri1.tstamp = std::chrono::system_clock::time_point(std::chrono::microseconds(5));
        qri1.response_delay = std::chrono::microseconds(10);
        qri1.qname = 5;
        qri1.signature = 6;
        qri1.query_size = 10;
        qri1.response_size = 20;
        qri1.query_extra_info = make_unique<QueryResponseExtraInfo>();
        qri1.query_extra_info->questions_list = 12;
        qri1.query_extra_info->answers_list = 13;
        qri1.query_extra_info->authority_list = 14;
        qri1.query_extra_info->additional_list = 15;

        qri1.response_extra_info = make_unique<QueryResponseExtraInfo>();
        qri1.response_extra_info->questions_list = 16;
        qri1.response_extra_info->answers_list = 17;
        qri1.response_extra_info->authority_list = 18;
        qri1.response_extra_info->additional_list = 19;

        WHEN("values are encoded")
        {
            TestCborEncoder tcbe;
            qri1.writeCbor(tcbe, std::chrono::system_clock::time_point(std::chrono::microseconds(0)));
            tcbe.flush();

            THEN("the encoding is as expected")
            {
                constexpr uint8_t EXPECTED[] =
                    {
                        (5 << 5) | 31,
                        find_query_response_index(QueryResponseField::time_useconds), 5,
                        find_query_response_index(QueryResponseField::client_address_index), 1,
                        find_query_response_index(QueryResponseField::client_port), 2,
                        find_query_response_index(QueryResponseField::transaction_id), 21,
                        find_query_response_index(QueryResponseField::query_signature_index), 6,
                        find_query_response_index(QueryResponseField::client_hoplimit), 20,
                        find_query_response_index(QueryResponseField::delay_useconds), 10,
                        find_query_response_index(QueryResponseField::query_name_index), 5,
                        find_query_response_index(QueryResponseField::query_size), 10,
                        find_query_response_index(QueryResponseField::response_size), 20,
                        find_query_response_index(QueryResponseField::query_extended),
                        (5 << 5) | 31,
                        0, 12,
                        1, 13,
                        2, 14,
                        3, 15,
                        0xff,
                        find_query_response_index(QueryResponseField::response_extended),
                        (5 << 5) | 31,
                        0, 16,
                        1, 17,
                        2, 18,
                        3, 19,
                        0xff,
                        0xff
                    };

                REQUIRE(tcbe.compareBytes(EXPECTED, sizeof(EXPECTED)));
            }
        }
    }
}

SCENARIO("HeaderList items can be written", "[block]")
{
    GIVEN("A sample HeaderList")
    {
        HeaderList<IntItem> hl;
        IntItem ii;
        ii.val = 1;
        hl.add(ii);
        ii.val = 2;
        hl.add(ii);
        ii.val = 3;
        hl.add(ii);

        WHEN("values are encoded")
        {
            TestCborEncoder tcbe;
            hl.writeCbor(tcbe);
            tcbe.flush();

            THEN("the encoding is as expected")
            {
                const uint8_t EXPECTED[] =
                    {
                        (4 << 5) | 31,
                        1,
                        2,
                        3,
                        0xff
                    };

                REQUIRE(tcbe.compareBytes(EXPECTED, sizeof(EXPECTED)));
            }
        }
    }
}

SCENARIO("BlockData items can be written", "[block]")
{
    GIVEN("A sample BlockData")
    {
        BlockData cd;
        cd.earliest_time = std::chrono::system_clock::time_point(std::chrono::seconds(1));

        WHEN("values are encoded")
        {
            TestCborEncoder tcbe;
            cd.writeCbor(tcbe);
            tcbe.flush();

            THEN("the encoding is as expected")
            {
                const uint8_t EXPECTED[] =
                    {
                        (5 << 5) | 31,
                        0, (5 << 5) | 1, 1, (4 << 5) | 2, 1, 0,

                        1,
                        (5 << 5) | 31,
                        0, 0,
                        1, 0,
                        2, 0,
                        3, 0,
                        4, 0,
                        10, 0,
                        11, 0,
                        12, 0,
                        13, 0,
                        14, 0,
                        0xff,

                        2,
                        (5 << 5) | 31,
                        0, (4 << 5) | 31, 0xff,
                        1, (4 << 5) | 31, 0xff,
                        2, (4 << 5) | 31, 0xff,
                        3, (4 << 5) | 31, 0xff,
                        4, (4 << 5) | 31, 0xff,
                        5, (4 << 5) | 31, 0xff,
                        6, (4 << 5) | 31, 0xff,
                        7, (4 << 5) | 31, 0xff,
                        0xff,

                        3,
                        (4 << 5) | 31,
                        0xff,

                        4,
                        (4 << 5) | 31,
                        0xff,

                        0xff
                    };

                REQUIRE(tcbe.compareBytes(EXPECTED, sizeof(EXPECTED)));
            }
        }
    }
}

SCENARIO("BlockData max items works", "[block]")
{
    GIVEN("A sample BlockData")
    {
        QueryResponseItem qri1;
        QueryResponseItem qri2;
        BlockData cd1(1);
        BlockData cd2(2);
        cd1.query_response_items.push_back(std::move(qri1));
        cd2.query_response_items.push_back(std::move(qri2));

        WHEN("a value is encoded")
        {
            TestCborEncoder tcbe;
            cd1.writeCbor(tcbe);
            cd2.writeCbor(tcbe);

            THEN("full report is as expected")
            {
                REQUIRE(cd1.is_full());
                REQUIRE(!cd2.is_full());
            }
        }
    }
}

SCENARIO("IndexVectorItems can be read", "[block]")
{
    GIVEN("A test CBOR decoder and sample vectors")
    {
        TestCborDecoder tcbd;
        std::vector<index_t> v1{1, 2, 3, 4};
        std::vector<index_t> v2{1, 2, 3, 5};
        IndexVectorItem iv1, iv2;
        iv1.vec = v1;
        iv2.vec = v2;

        WHEN("decoder is given encoded index vector data")
        {
            const std::vector<uint8_t> INPUT =
                {
                    (4 << 5) | 31,
                    1, 2, 3, 4,
                    0xff,
                    (4 << 5) | 31,
                    1, 2, 3, 5,
                    0xff,
                };
            tcbd.set_bytes(INPUT);

            THEN("decoder input is correct")
            {
                IndexVectorItem iv1_r, iv2_r;
                block_cbor::FileVersionFields fields;
                iv1_r.readCbor(tcbd, fields);
                iv2_r.readCbor(tcbd, fields);

                REQUIRE(iv1.vec == iv1_r.vec);
                REQUIRE(iv2.vec == iv2_r.vec);
            }
        }
    }
}

SCENARIO("ByteStringItems can be read", "[block]")
{
    GIVEN("A test CBOR decoder and sample strings")
    {
        TestCborDecoder tcbd;
        ByteStringItem si1, si2;
        si1.str = "Hello"_b;
        si2.str = "World"_b;

        WHEN("decoder is given encoded string data")
        {
            const std::vector<uint8_t> INPUT =
                {
                    (2 << 5) | 5,
                    'H', 'e', 'l', 'l', 'o',
                    (2 << 5) | 5,
                    'W', 'o', 'r', 'l', 'd',
                };
            tcbd.set_bytes(INPUT);

            THEN("decoder input is correct")
            {
                ByteStringItem s1_r, s2_r;
                block_cbor::FileVersionFields fields;
                s1_r.readCbor(tcbd, fields);
                s2_r.readCbor(tcbd, fields);

                REQUIRE(si1.str == s1_r.str);
                REQUIRE(si2.str == s2_r.str);
            }
        }
    }
}

SCENARIO("IPAddressItems can be read", "[block]")
{
    GIVEN("A test CBOR decoder and sample IP addresses")
    {
        TestCborDecoder tcbd;
        Tins::IPv4Address a4("193.0.29.226");
        Tins::IPv6Address a6("2001:67c:64:42:bdcd:34ce:2801:9686");
        IPAddressItem ai1, ai2;
        ai1.addr = IPAddress(a4);
        ai2.addr = IPAddress(a6);

        WHEN("decoder is given encoded IP address data")
        {
            const std::vector<uint8_t> INPUT =
                {
                    (2 << 5) | 4,
                    193, 0, 29, 226,
                    (2 << 5) | 16,
                    0x20, 0x01, 0x06, 0x7c, 0x00, 0x64, 0x00, 0x42,
                    0xbd, 0xcd, 0x34, 0xce, 0x28, 0x01, 0x96, 0x86,
                };
            tcbd.set_bytes(INPUT);

            THEN("decoder input is correct")
            {
                IPAddressItem ai1_r, ai2_r;
                block_cbor::FileVersionFields fields;
                ai1_r.readCbor(tcbd, fields);
                ai2_r.readCbor(tcbd, fields);

                REQUIRE(ai1.addr == ai1_r.addr);
                REQUIRE(ai2.addr == ai2_r.addr);
            }
        }
    }
}

SCENARIO("ClassTypes can be read", "[block]")
{
    GIVEN("A test CBOR decoder and sample class type info")
    {
        TestCborDecoder tcbd;
        ClassType ct1, ct2;
        ct1.qclass = ct2.qclass = CaptureDNS::INTERNET;
        ct1.qtype = CaptureDNS::A;
        ct2.qtype = CaptureDNS::CNAME;

        WHEN("decoder is given encoded class type data")
        {
            const std::vector<uint8_t> INPUT =
                {
                    (5 << 5) | 2,
                    0, 1,
                    1, 1,
                    (5 << 5) | 2,
                    0, 5,
                    1, 1,
                };
            tcbd.set_bytes(INPUT);

            THEN("decoder input is correct")
            {
                ClassType ct1_r, ct2_r;
                block_cbor::FileVersionFields fields;
                ct1_r.readCbor(tcbd, fields);
                ct2_r.readCbor(tcbd, fields);

                REQUIRE(ct1 == ct1_r);
                REQUIRE(ct2 == ct2_r);
            }
        }
    }
}

SCENARIO("Questions can be read", "[block]")
{
    GIVEN("A test CBOR decoder and sample question data")
    {
        TestCborDecoder tcbd;
        Question q1, q2;
        q1.qname = 1;
        q2.qname = 2;
        q1.classtype = 20;
        q2.classtype = 19;

        WHEN("decoder is given encoded question data")
        {
            const std::vector<uint8_t> INPUT =
                {
                    (5 << 5) | 2,
                    0, 1,
                    1, 20,
                    (5 << 5) | 2,
                    0, 2,
                    1, 19,
                };
            tcbd.set_bytes(INPUT);

            THEN("decoder input is correct")
            {
                Question q1_r, q2_r;
                block_cbor::FileVersionFields fields;
                q1_r.readCbor(tcbd, fields);
                q2_r.readCbor(tcbd, fields);

                REQUIRE(q1 == q1_r);
                REQUIRE(q2 == q2_r);
            }
        }
    }
}

SCENARIO("ResourceRecords can be read", "[block]")
{
    GIVEN("A test CBOR decoder and sample resource record data")
    {
        TestCborDecoder tcbd;
        ResourceRecord rr1, rr2;
        rr1.name = rr2.name = 1;
        rr1.classtype = 12;
        rr2.classtype = 13;
        rr1.ttl = rr2.ttl = 10;
        rr1.rdata = rr2.rdata = 11;

        WHEN("decoder is given encoded question data")
        {
            const std::vector<uint8_t> INPUT =
                {
                    (5 << 5) | 4,
                    0, 1,
                    1, 12,
                    2, 10,
                    3, 11,
                    (5 << 5) | 4,
                    0, 1,
                    1, 13,
                    2, 10,
                    3, 11,
                };
            tcbd.set_bytes(INPUT);

            THEN("decoder input is correct")
            {
                ResourceRecord rr1_r, rr2_r;
                block_cbor::FileVersionFields fields;
                rr1_r.readCbor(tcbd, fields);
                rr2_r.readCbor(tcbd, fields);

                REQUIRE(rr1 == rr1_r);
                REQUIRE(rr2 == rr2_r);
            }
        }
    }
}

SCENARIO("QuerySignatures can be read", "[block]")
{
    GIVEN("A test CBOR decoder and sample query signature data")
    {
        TestCborDecoder tcbd;
        QuerySignature qs1;
        qs1.server_address = 1;
        qs1.server_port = 2;
        qs1.transport_flags = 3;
        qs1.qr_flags = 0x1f;
        qs1.qdcount = 1;
        qs1.query_rcode = 22;
        qs1.response_rcode = 23;
        qs1.query_opcode = 2;
        qs1.query_edns_version = 0;
        qs1.query_edns_payload_size = 22;
        qs1.query_opt_rdata = 4;
        qs1.dns_flags = 8;
        qs1.query_classtype = 3;
        qs1.query_ancount = 2;
        qs1.query_arcount = 3;
        qs1.query_nscount = 4;

        WHEN("decoder is given encoded question data")
        {
            constexpr uint8_t INPUT[] =
                {
                    (5 << 5) | 31,
                    find_query_signature_index(QuerySignatureField::server_address_index), 1,
                    find_query_signature_index(QuerySignatureField::server_port), 2,
                    find_query_signature_index(QuerySignatureField::transport_flags), 3,
                    find_query_signature_index(QuerySignatureField::qr_dns_flags), 8,
                    find_query_signature_index(QuerySignatureField::qr_sig_flags), (0 << 5) | 24, 0x1f,
                    find_query_signature_index(QuerySignatureField::query_qd_count), 1,
                    find_query_signature_index(QuerySignatureField::query_classtype_index), 3,
                    find_query_signature_index(QuerySignatureField::query_rcode), 22,
                    find_query_signature_index(QuerySignatureField::query_opcode), 2,
                    find_query_signature_index(QuerySignatureField::query_an_count), 2,
                    find_query_signature_index(QuerySignatureField::query_ar_count), 3,
                    find_query_signature_index(QuerySignatureField::query_ns_count), 4,
                    find_query_signature_index(QuerySignatureField::edns_version), 0,
                    find_query_signature_index(QuerySignatureField::udp_buf_size), 22,
                    find_query_signature_index(QuerySignatureField::opt_rdata_index), 4,
                    find_query_signature_index(QuerySignatureField::response_rcode), 23,
                    0xff
                };
            std::vector<uint8_t> bytes(INPUT, INPUT + sizeof(INPUT));
            tcbd.set_bytes(bytes);

            THEN("decoder input is correct")
            {
                QuerySignature qs1_r;
                block_cbor::FileVersionFields fields;
                qs1_r.readCbor(tcbd, fields);

                REQUIRE(qs1 == qs1_r);
            }
        }
    }
}

SCENARIO("QueryResponseItems can be read", "[block]")
{
    GIVEN("A test CBOR decoder and sample query response item data")
    {
        TestCborDecoder tcbd;
        QueryResponseItem qri1;
        qri1.qr_flags = 7;
        qri1.client_address = 1;
        qri1.client_port = 2;
        qri1.hoplimit = 20;
        qri1.id = 21;
        qri1.tstamp = std::chrono::system_clock::time_point(std::chrono::microseconds(5));
        qri1.response_delay = std::chrono::microseconds(10);
        qri1.qname = 5;
        qri1.signature = 6;
        qri1.query_size = 10;
        qri1.response_size = 20;
        qri1.query_extra_info = make_unique<QueryResponseExtraInfo>();
        qri1.query_extra_info->questions_list = 12;
        qri1.query_extra_info->answers_list = 13;
        qri1.query_extra_info->authority_list = 14;
        qri1.query_extra_info->additional_list = 15;

        qri1.response_extra_info = make_unique<QueryResponseExtraInfo>();
        qri1.response_extra_info->questions_list = 16;
        qri1.response_extra_info->answers_list = 17;
        qri1.response_extra_info->authority_list = 18;
        qri1.response_extra_info->additional_list = 19;

        WHEN("decoder is given encoded query response item data")
        {
            constexpr uint8_t INPUT[] =
                {
                    (5 << 5) | 31,
                    find_query_response_index(QueryResponseField::time_useconds), 5,
                    find_query_response_index(QueryResponseField::client_address_index), 1,
                    find_query_response_index(QueryResponseField::client_port), 2,
                    find_query_response_index(QueryResponseField::transaction_id), 21,
                    find_query_response_index(QueryResponseField::query_signature_index), 6,
                    find_query_response_index(QueryResponseField::client_hoplimit), 20,
                    find_query_response_index(QueryResponseField::delay_useconds), 10,
                    find_query_response_index(QueryResponseField::query_name_index), 5,
                    find_query_response_index(QueryResponseField::query_size), 10,
                    find_query_response_index(QueryResponseField::response_size), 20,
                    find_query_response_index(QueryResponseField::query_extended),
                    (5 << 5) | 31,
                    0, 12,
                    1, 13,
                    2, 14,
                    3, 15,
                    0xff,
                    find_query_response_index(QueryResponseField::response_extended),
                    (5 << 5) | 31,
                    0, 16,
                    1, 17,
                    2, 18,
                    3, 19,
                    0xff,
                    0xff
                };
            std::vector<uint8_t> bytes(INPUT, INPUT + sizeof(INPUT));
            tcbd.set_bytes(bytes);

            THEN("decoder input is correct")
            {
                QueryResponseItem qri1_r;
                block_cbor::FileVersionFields fields;
                qri1_r.readCbor(tcbd, std::chrono::system_clock::time_point(std::chrono::microseconds(0)), fields);

                REQUIRE(qri1.qr_flags == qri1_r.qr_flags);
                REQUIRE(qri1.client_address == qri1_r.client_address);
                REQUIRE(qri1.client_port == qri1_r.client_port);
                REQUIRE(qri1.hoplimit == qri1_r.hoplimit);
                REQUIRE(qri1.id == qri1_r.id);
                REQUIRE(qri1.tstamp == qri1_r.tstamp);
                REQUIRE(qri1.response_delay == qri1_r.response_delay);
                REQUIRE(qri1.qname == qri1_r.qname);
                REQUIRE(qri1.signature == qri1_r.signature);
                REQUIRE(qri1.query_size == qri1_r.query_size);
                REQUIRE(qri1.response_size == qri1_r.response_size);
                REQUIRE(qri1.query_extra_info->questions_list == qri1_r.query_extra_info->questions_list);
                REQUIRE(qri1.query_extra_info->answers_list == qri1_r.query_extra_info->answers_list);
                REQUIRE(qri1.query_extra_info->authority_list == qri1_r.query_extra_info->authority_list);
                REQUIRE(qri1.query_extra_info->additional_list == qri1_r.query_extra_info->additional_list);
                REQUIRE(qri1.response_extra_info->questions_list == qri1_r.response_extra_info->questions_list);
                REQUIRE(qri1.response_extra_info->answers_list == qri1_r.response_extra_info->answers_list);
                REQUIRE(qri1.response_extra_info->authority_list == qri1_r.response_extra_info->authority_list);
                REQUIRE(qri1.response_extra_info->additional_list == qri1_r.response_extra_info->additional_list);
            }
        }
    }
}

SCENARIO("AddressEventCounts can be read", "[block]")
{
    GIVEN("A test CBOR decoder and sample AddressEventCount data")
    {
        TestCborDecoder tcbd;
        AddressEventCount aec1;
        aec1.aei.type = AddressEvent::EventType::TCP_RESET;
        aec1.aei.code = 11;
        aec1.aei.address = 10;
        aec1.count = 22;

        WHEN("decoder is given encoded question data")
        {
            const std::vector<uint8_t> INPUT =
                {
                    (5 << 5) | 4,
                    0, 0,
                    1, 11,
                    2, 10,
                    3, 22,
                };
            tcbd.set_bytes(INPUT);

            THEN("decoder input is correct")
            {
                AddressEventCount aec1_r;
                block_cbor::FileVersionFields fields;
                aec1_r.readCbor(tcbd, fields);

                REQUIRE(aec1.aei == aec1_r.aei);
                REQUIRE(aec1.count == aec1_r.count);
            }
        }
    }
}

SCENARIO("HeaderList items can be read", "[block]")
{
    GIVEN("A test CBOR decoder and sample header list item data")
    {
        TestCborDecoder tcbd;

        WHEN("decoder is given encoded header list item data")
        {
            const std::vector<uint8_t> INPUT =
                {
                    (4 << 5) | 31,
                    1,
                    2,
                    3,
                    0xff
                };
            tcbd.set_bytes(INPUT);

            THEN("decoder input is correct")
            {
                HeaderList<IntItem> hl_r;
                block_cbor::FileVersionFields fields;
                hl_r.readCbor(tcbd, fields);

                REQUIRE(hl_r.size() == 3);
                REQUIRE(hl_r[1].val == 1);
                REQUIRE(hl_r[2].val == 2);
                REQUIRE(hl_r[3].val == 3);
            }
        }
    }
}

SCENARIO("BlockData items can be read", "[block]")
{
    GIVEN("A test CBOR decoder and sample header list item data")
    {
        TestCborDecoder tcbd;
        BlockData cd;
        cd.earliest_time = std::chrono::system_clock::time_point(std::chrono::seconds(1));

        WHEN("decoder is given encoded block data")
        {
            const std::vector<uint8_t> INPUT =
                {
                    (5 << 5) | 31,
                    0, (5 << 5) | 2, 0, 1, 1, (4 << 5) | 2, 1, 0,

                    1,
                    (5 << 5) | 31,
                    0, 0,
                    1, 0,
                    2, 0,
                    3, 0,
                    4, 0,
                    5, 0,
                    6, 0,
                    7, 0,
                    8, 0,
                    9, 0,
                    0xff,

                    2,
                    (5 << 5) | 31,
                    0, (4 << 5) | 31, 0xff,
                    1, (4 << 5) | 31, 0xff,
                    2, (4 << 5) | 31, 0xff,
                    3, (4 << 5) | 31, 0xff,
                    4, (4 << 5) | 31, 0xff,
                    5, (4 << 5) | 31, 0xff,
                    6, (4 << 5) | 31, 0xff,
                    7, (4 << 5) | 31, 0xff,
                    0xff,

                    3,
                    (4 << 5) | 31,
                    0xff,

                    4,
                    (4 << 5) | 31,
                    0xff,

                    0xff
                };
            tcbd.set_bytes(INPUT);

            THEN("decoder input is correct")
            {
                BlockData cd_r;
                block_cbor::FileVersionFields fields;
                cd_r.readCbor(tcbd, fields);

                REQUIRE(cd_r.earliest_time == cd.earliest_time);
            }
        }
    }
}
