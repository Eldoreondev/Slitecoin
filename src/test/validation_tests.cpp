// Copyright (c) 2014-2019 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>
#include <net.h>
#include <signet.h>
#include <validation.h>

#include <test/util/setup_common.h>

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(validation_tests, TestingSetup)

static void TestBlockSubsidyHalvings(const Consensus::Params& consensusParams)
{
    const CAmount nNormalSubsidy = 50 * COIN;

    // Genesis is unspendable and carries no spendable mining subsidy.
    BOOST_CHECK_EQUAL(GetBlockSubsidy(0, consensusParams), 0);

    // Height 1 is the STC bootstrap/allocation block. It receives
    // 100 STC: the normal 50 STC subsidy plus 50 STC replacing
    // the otherwise unspendable genesis-era subsidy.
    BOOST_CHECK_EQUAL(GetBlockSubsidy(1, consensusParams), 100 * COIN);

    // Normal mining starts from height 2 at 50 STC.
    BOOST_CHECK_EQUAL(GetBlockSubsidy(2, consensusParams), nNormalSubsidy);

    for (int nHalvings = 0; nHalvings < 64; nHalvings++) {
        const int nHeight = 2 + nHalvings * consensusParams.nSubsidyHalvingInterval;
        CAmount nSubsidy = GetBlockSubsidy(nHeight, consensusParams);

        if (nSubsidy == 0) {
            break;
        }

        BOOST_CHECK(nSubsidy <= nNormalSubsidy);
    }

    BOOST_CHECK_EQUAL(
        GetBlockSubsidy(64 * consensusParams.nSubsidyHalvingInterval, consensusParams),
        0);
}

static void TestBlockSubsidyHalvings(int nSubsidyHalvingInterval)
{
    Consensus::Params consensusParams;
    consensusParams.nSubsidyHalvingInterval = nSubsidyHalvingInterval;
    TestBlockSubsidyHalvings(consensusParams);
}

BOOST_AUTO_TEST_CASE(block_subsidy_test)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    TestBlockSubsidyHalvings(chainParams->GetConsensus()); // As in main
    TestBlockSubsidyHalvings(150); // As in regtest
    TestBlockSubsidyHalvings(1000); // Just another interval
}

BOOST_AUTO_TEST_CASE(subsidy_limit_test)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    const auto& consensus = chainParams->GetConsensus();

    // STC spendable mining allocation:
    // - Height 1 bootstrap block: 100 STC.
    // - Height 2 onward: 50 STC initial subsidy, halving every
    //   nSubsidyHalvingInterval blocks.
    //
    // The 100 STC bootstrap reward restores the 50 STC subsidy that
    // would otherwise have been permanently unspendable in genesis.
    CAmount totalMiningSubsidy = GetBlockSubsidy(1, consensus);
    BOOST_CHECK_EQUAL(totalMiningSubsidy, 100 * COIN);
    BOOST_CHECK(MoneyRange(totalMiningSubsidy));

    for (int halvings = 0; halvings < 64; ++halvings) {
        const CAmount nSubsidy = 50 * COIN >> halvings;
        if (nSubsidy == 0) {
            break;
        }

        const int nStartHeight =
            2 + halvings * consensus.nSubsidyHalvingInterval;

        CAmount blocksInEra = consensus.nSubsidyHalvingInterval;
        if (halvings == 0) {
            // Height 0 is genesis and height 1 is the bootstrap block.
            // The normal 50 STC era therefore starts at height 2.
            blocksInEra -= 2;
        }

        totalMiningSubsidy += nSubsidy * blocksInEra;

        BOOST_CHECK(nSubsidy <= 50 * COIN);
        BOOST_CHECK(MoneyRange(totalMiningSubsidy));

        (void)nStartHeight;
    }

    const CAmount targetMiningAllocation = 42000000 * COIN;
    const CAmount roundingDifference =
        targetMiningAllocation - totalMiningSubsidy;

    // Integer halving produces a final rounding difference of
    // 0.04620000 STC.
    BOOST_CHECK_EQUAL(roundingDifference, 4620000);
}

BOOST_AUTO_TEST_SUITE_END()
