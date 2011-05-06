/*
 *   SniffJoke is a software able to confuse the Internet traffic analysis,
 *   developed with the aim to improve digital privacy in communications and
 *   to show and test some securiy weakness in traffic analysis software.
 *
 * Copyright (C) 2010, 2011 vecna <vecna@delirandom.net>
 *                          evilaliv3 <giovanni.pellerano@evilaliv3.org>
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "hardcodedDefines.h"

#include "OptionPool.h"

#include "UserConf.h"

extern auto_ptr<UserConf> userconf;

corruption_t OptionPool::lineParser(FILE *flow, uint32_t optLooked)
{
    corruption_t retval = CORRUPTUNASSIGNED;
    char line[MEDIUMBUF];
    uint32_t linecnt = 0;

    uint32_t readedIndex, readedCorruption;

    do
    {
        fgets(line, MEDIUMBUF, flow);
        ++linecnt;

        if (feof(flow))
            break;

        if (strlen(line) < 2 || line[0] == '#')
            continue;

        sscanf(line, "%u,%u", &readedIndex, &readedCorruption);

        if (readedIndex >= SUPPORTED_OPTIONS)
            RUNTIME_EXCEPTION("in option file invalid index at line %u", linecnt);

        if (readedIndex == optLooked)
            retval = (corruption_t) readedCorruption;
        else
            RUNTIME_EXCEPTION("found index %u instead of the expected %u (line %u)",
                              readedIndex, optLooked, linecnt);

    }
    while (retval == CORRUPTUNASSIGNED);

    if (retval == CORRUPTUNASSIGNED)
        RUNTIME_EXCEPTION("unable to found option index %u in the option config file", optLooked);

    LOG_VERBOSE("option index %u found value corruption value of %u", optLooked, (uint8_t) retval);

    return retval;
}

OptionPool::OptionPool()
{
    /* WARNING: don't touch this piece of code if you are HIGH! */
    pool.push_back(new Io_NOOP(true));
    pool.push_back(new Io_EOL(true));
    pool.push_back(new Io_TIMESTAMP(true));
    pool.push_back(new Io_TIMESTOVERFLOW(false));
    /* WARNING: don't touch this piece of code if you are SMOKED! */
    pool.push_back(new Io_LSRR(true));
    pool.push_back(new Io_RR(true));
    pool.push_back(new Io_RA(true));
    pool.push_back(new Io_CIPSO(true));
    /* WARNING: don't touch this piece of code if you are DRUNKED! */
    pool.push_back(new Io_SEC(true));
    pool.push_back(new Io_SID(true));
    pool.push_back(new To_NOP(true));
    pool.push_back(new To_EOL(true));
    /* WARNING: don't touch this piece of code if you are LAZY! */
    pool.push_back(new To_MD5SIG(false));
    pool.push_back(new To_PAWSCORRUPT(false));
    pool.push_back(new To_TIMESTAMP(false));
    pool.push_back(new To_MSS(false));
    /* WARNING: don't touch this piece of code if you are BOXXY! */
    pool.push_back(new To_SACK(false));
    pool.push_back(new To_SACKPERM(false));
    pool.push_back(new To_WINDOW(false));
    /* WARNING, because the order MATTERS! this constructor column 
     * is derived from the incremental order of the value in hardcodedDefines.h */

    /* when is loaded the single plugin HDRoptions_probe, the option loader is instanced w/ NULL */
    if (!(userconf->runcfg.onlyplugin[0] != 0x00 && !memcmp(userconf->runcfg.onlyplugin, IPTCPOPT_TEST_PLUGIN, strlen(IPTCPOPT_TEST_PLUGIN))))
    {
        /* loading the configuration file, containings which option bring corruption for your ISP */
        /* NOW - sets with the default used by vecna & evilaliv3 */
        /* THESE DATA HAS TO BE LOADED FROM A Location-SPECIFIC CONFIGUATION FILE */
        corruption_t writUsage;

        FILE *optInput = fopen(FILE_IPTCPOPT_CONF, "r");
        if (optInput == NULL)
            RUNTIME_EXCEPTION("unable to open in reading options configuration %s: %s", FILE_IPTCPOPT_CONF, strerror(errno));

        for (uint8_t sjI = 0; sjI < SUPPORTED_OPTIONS; ++sjI)
        {
            writUsage = lineParser(optInput, sjI);
            pool[sjI]->optionConfigure(writUsage);
        }

        fclose(optInput);

        LOG_DEBUG("options configuration loaded correctly from %s: %u defs", FILE_IPTCPOPT_CONF, SUPPORTED_OPTIONS);
    }
    else
    {
        /* testing modality - all options are loaded without a corruption definitions */
        LOG_ALL("option configuration not supplied! Initializing in testing mode");
    }
}

OptionPool::~OptionPool()
{
    /* TO BE COMPLETED */
}

IPTCPopt *OptionPool::get(uint32_t sjOptIndex)
{
    if (sjOptIndex >= SUPPORTED_OPTIONS)
        RUNTIME_EXCEPTION("Invalid index request: %u on %u available", sjOptIndex, SUPPORTED_OPTIONS);

    return pool[sjOptIndex];
}