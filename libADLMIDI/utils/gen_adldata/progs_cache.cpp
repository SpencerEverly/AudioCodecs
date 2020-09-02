#include "progs_cache.h"

#include "file_formats/load_ail.h"
#include "file_formats/load_bisqwit.h"
#include "file_formats/load_bnk2.h"
#include "file_formats/load_bnk.h"
#include "file_formats/load_ibk.h"
#include "file_formats/load_jv.h"
#include "file_formats/load_op2.h"
#include "file_formats/load_tmb.h"
#include "file_formats/load_wopl.h"
#include "file_formats/load_ea.h"


InstBuffer MakeNoSoundIns1()
{
    InstBuffer nosnd;
    uint8_t d[] = {0x00, 0x10, 0x07, 0x07, 0xF7, 0xF7, 0x00, 0x00, 0xFF, 0xFF, 0x00};
    std::memcpy(nosnd.data, d, 11);
    return nosnd;
}

void BanksDump::toOps(const InstBuffer_t &inData, BanksDump::Operator *outData, size_t begin)
{
    outData[begin + 0].d_E862 =
            uint_fast32_t(inData.op1_wave << 24)
          + uint_fast32_t(inData.op1_susrel << 16)
          + uint_fast32_t(inData.op1_atdec << 8)
          + uint_fast32_t(inData.op1_amvib << 0);
    outData[begin + 1].d_E862 =
            uint_fast32_t(inData.op2_wave << 24)
          + uint_fast32_t(inData.op2_susrel << 16)
          + uint_fast32_t(inData.op2_atdec << 8)
          + uint_fast32_t(inData.op2_amvib << 0);
    outData[begin + 0].d_40 = inData.op1_ksltl;
    outData[begin + 1].d_40 = inData.op2_ksltl;
}

size_t BanksDump::initBank(size_t bankId, const std::string &title, uint_fast16_t bankSetup)
{
    for(size_t bID = 0; bID < banks.size(); bID++)
    {
        BankEntry &be = banks[bID];
        if(bankId == be.bankId)
        {
            be.bankTitle = title;
            be.bankSetup = bankSetup;
            return bID;
        }
    }

    size_t bankIndex = banks.size();
    banks.emplace_back();
    BankEntry &b = banks.back();

    b.bankId = bankId;
    b.bankTitle = title;
    b.bankSetup = bankSetup;
    return bankIndex;
}

void BanksDump::addMidiBank(size_t bankId, bool percussion, BanksDump::MidiBank b)
{
    assert(bankId < banks.size());
    BankEntry &be = banks[bankId];

    auto it = std::find(midiBanks.begin(), midiBanks.end(), b);
    if(it == midiBanks.end())
    {
        b.midiBankId = midiBanks.size();
        midiBanks.push_back(b);
    }
    else
    {
        b.midiBankId = it->midiBankId;
    }

    if(percussion)
        be.percussion.push_back(b.midiBankId);
    else
        be.melodic.push_back(b.midiBankId);
}

void BanksDump::addInstrument(BanksDump::MidiBank &bank, size_t patchId,
                              BanksDump::InstrumentEntry e,
                              BanksDump::Operator *ops,
                              const std::string &meta)
{
    assert(patchId < 128);
    size_t opsCount = ((e.instFlags & InstrumentEntry::WOPL_Ins_4op) != 0 ||
            (e.instFlags & InstrumentEntry::WOPL_Ins_Pseudo4op) != 0) ?
                4 : 2;

    if((e.instFlags & InstrumentEntry::WOPL_Ins_IsBlank) != 0)
    {
        bank.instruments[patchId] = -1;
        return;
    }

    for(size_t op = 0; op < opsCount; op++)
    {
        Operator o = ops[op];
        auto it = std::find(operators.begin(), operators.end(), o);
        if(it == operators.end())
        {
            o.opId = operators.size();
            e.ops[op] = static_cast<int_fast32_t>(o.opId);
            operators.push_back(o);
        }
        else
        {
            e.ops[op] = static_cast<int_fast32_t>(it->opId);
        }
    }

    auto it = std::find(instruments.begin(), instruments.end(), e);
    if(it == instruments.end())
    {
        e.instId = instruments.size();
        e.instMetas.push_back(meta + "_" + std::to_string(patchId));
        instruments.push_back(e);
    }
    else
    {
        e.instId = it->instId;
        e.instMetas.push_back(meta + "_" + std::to_string(patchId));
        it->instMetas.push_back(meta + "_" + std::to_string(patchId));
    }
    bank.instruments[patchId] = static_cast<int_fast32_t>(e.instId);
}

void BanksDump::exportBanks(const std::string &outPath, const std::string &headerName)
{
    FILE *out = std::fopen(outPath.c_str(), "w");

    std::fprintf(out, "/**********************************************************\n"
                      "    This file is generated by `gen_adldata` automatically\n"
                      "                  Don't edit it directly!\n"
                      "        To modify content of this file, modify banks\n"
                      "          and re-run the `gen_adldata` build step.\n"
                      "***********************************************************/\n\n"
                      "#include \"%s\"\n\n\n", headerName.c_str());

    std::fprintf(out, "const size_t g_embeddedBanksCount = %zu;\n\n", banks.size());
    std::fprintf(out, "const BanksDump::BankEntry g_embeddedBanks[] =\n"
                      "{\n");

    std::vector<size_t> bankNumberLists;

    for(const BankEntry &be : banks)
    {
        std::fprintf(out, "\t{0x%04lX, %zu, %zu, \"%s\", ",
                                   be.bankSetup,
                                   be.melodic.size(),
                                   be.percussion.size(),
                                   be.bankTitle.c_str());

        fprintf(out, "%zu, ", bankNumberLists.size()); // Use offset to point the common array of bank IDs
        for(const size_t &me : be.melodic)
            bankNumberLists.push_back(me);

        fprintf(out, "%zu", bankNumberLists.size());
        for(const size_t &me : be.percussion)
            bankNumberLists.push_back(me);

        std::fprintf(out, "},\n");
    }

    std::fprintf(out, "};\n\n");

    std::fprintf(out, "const char* const g_embeddedBankNames[] =\n"
                      "{\n\t");
    {
        bool commaNeeded = false;
        size_t operatorEntryCounter = 0;
        for(const BankEntry &be : banks)
        {
            if(commaNeeded)
                std::fprintf(out, ", ");
            else
                commaNeeded = true;
            operatorEntryCounter++;
            if(operatorEntryCounter >= 25)
            {
                std::fprintf(out, "\n");
                operatorEntryCounter = 0;
            }
            if(operatorEntryCounter == 0)
                std::fprintf(out, "\t");
            std::fprintf(out, "g_embeddedBanks[%zu].title", be.bankId);
        }
        std::fprintf(out, ",\n\tNULL"); // Make a null entry as finalizer
    }
    std::fprintf(out, "\n};\n\n");

    std::fprintf(out, "const size_t g_embeddedBanksMidiIndex[] =\n"
                      "{");
    {
        bool commaNeeded = false;
        for(const size_t &me : bankNumberLists)
        {
            if(commaNeeded)
                std::fprintf(out, ",");
            else
                commaNeeded = true;
            std::fprintf(out, "%zu", me);
        }
    }
    std::fprintf(out, "};\n\n");

    std::fprintf(out, "const BanksDump::MidiBank g_embeddedBanksMidi[] =\n"
                      "{\n");
    for(const MidiBank &be : midiBanks)
    {
        bool commaNeeded = true;
        std::fprintf(out, "\t{%u,%u,", be.msb, be.lsb);

        std::fprintf(out, "{");
        commaNeeded = false;
        for(size_t i = 0; i < 128; i++)
        {
            if(commaNeeded)
                std::fprintf(out, ",");
            else
                commaNeeded = true;
            std::fprintf(out, "%ld", be.instruments[i]);
        }
        std::fprintf(out, "}");

        std::fprintf(out, "},\n");
    }
    std::fprintf(out, "};\n\n");


    std::fprintf(out, "const BanksDump::InstrumentEntry g_embeddedBanksInstruments[] =\n"
                      "{\n");
    for(const InstrumentEntry &be : instruments)
    {
        size_t opsCount = ((be.instFlags & InstrumentEntry::WOPL_Ins_4op) != 0 ||
                           (be.instFlags & InstrumentEntry::WOPL_Ins_Pseudo4op) != 0) ? 4 : 2;
        std::fprintf(out, "\t{%d,%d,%d,%u,%s%lX,%d,%s%lX,%s%lX,%s%lX,",
                     be.noteOffset1,
                     be.noteOffset2,
                     be.midiVelocityOffset,
                     be.percussionKeyNumber,
                     (be.instFlags == 0 ? "" : "0x"), be.instFlags, // for compactness, don't print "0x" when is zero
                     be.secondVoiceDetune,
                     (be.fbConn == 0 ? "" : "0x"), be.fbConn,
                     (be.delay_on_ms == 0 ? "" : "0x"), be.delay_on_ms,
                     (be.delay_off_ms == 0 ? "" : "0x"), be.delay_off_ms);

        if(opsCount == 4)
            std::fprintf(out, "{%ld,%ld,%ld,%ld} ",
                         be.ops[0], be.ops[1], be.ops[2], be.ops[3]);
        else
            std::fprintf(out, "{%ld,%ld}",
                         be.ops[0], be.ops[1]);

        std::fprintf(out, "},\n");
    }
    std::fprintf(out, "};\n\n");

    std::fprintf(out, "const BanksDump::Operator g_embeddedBanksOperators[] =\n"
                      "{\n");
    size_t operatorEntryCounter = 0;
    for(const Operator &be : operators)
    {
        if(operatorEntryCounter == 0)
            std::fprintf(out, "\t");
        std::fprintf(out, "{0x%07lX,%s%02lX},",
                     be.d_E862,
                     (be.d_40 == 0 ? "" : "0x"), be.d_40);
        operatorEntryCounter++;
        if(operatorEntryCounter >= 25)
        {
            std::fprintf(out, "\n");
            operatorEntryCounter = 0;
        }
    }
    std::fprintf(out, "\n};\n\n");

    std::fclose(out);
}

struct OpCheckData
{
    uint_fast8_t egEn;
    uint_fast8_t attack;
    uint_fast8_t decay;
    uint_fast8_t sustain;
    uint_fast8_t release;
    uint_fast8_t level;

    void setData(uint_fast32_t d_E862, uint_fast32_t d_40)
    {
        egEn    = static_cast<uint_fast8_t>(((d_E862 & 0xFF) >> 5) & 0x01);
        decay   = static_cast<uint_fast8_t>((d_E862 >> 8)  & 0x0F);
        attack  = static_cast<uint_fast8_t>((d_E862 >> 12) & 0x0F);
        release = static_cast<uint_fast8_t>((d_E862 >> 16) & 0x0F);
        sustain = static_cast<uint_fast8_t>((d_E862 >> 20) & 0x0F);
        level   = static_cast<uint_fast8_t>(d_40 & 0x3F);
    }

    bool isOpSilent(bool moreInfo)
    {
        // level=0x3f - silence
        // attack=0x00 - silence
        // attack=0x0F & sustain=0x0F & decay=0x0F - half-silence
        // attack=0x0F & decay=0x0F & release=0x00 & egOff - half-silence
        if(level == 0x3F)
        {
            if(moreInfo)
                std::fprintf(stdout, "== volume=0x3F ==\n");
            return true;
        }
        if(attack == 0x00)
        {
            if(moreInfo)
                std::fprintf(stdout, "== attack=0x00 ==\n");
            return true;
        }
        if(attack == 0x0F && sustain == 0x0F && decay == 0x0F)
        {
            if(moreInfo)
                std::fprintf(stdout, "== attack=0x0F, sustain=0x0F, decay=0x0F ==\n");
            return true;
        }
        if(attack == 0x0F && decay == 0x0F && release == 0x00 && !egEn)
        {
            if(moreInfo)
                std::fprintf(stdout, "== attack=0x0F, decay=0x0F, release=0x00, !egEn ==\n");
            return true;
        }
        return false;
    }
};

bool BanksDump::isSilent(const BanksDump &db, const BanksDump::InstrumentEntry &ins, bool moreInfo)
{
    bool isPseudo4ops = ((ins.instFlags & BanksDump::InstrumentEntry::WOPL_Ins_Pseudo4op) != 0);
    bool is4ops =       ((ins.instFlags & BanksDump::InstrumentEntry::WOPL_Ins_4op) != 0) && !isPseudo4ops;
    size_t opsNum = (is4ops || isPseudo4ops) ? 4 : 2;
    BanksDump::Operator ops[4];
    assert(ins.ops[0] >= 0);
    assert(ins.ops[1] >= 0);
    ops[0] = db.operators[ins.ops[0]];
    ops[1] = db.operators[ins.ops[1]];
    if(opsNum > 2)
    {
        assert(ins.ops[2] >= 0);
        assert(ins.ops[3] >= 0);
        ops[2] = db.operators[ins.ops[2]];
        ops[3] = db.operators[ins.ops[3]];
    }
    return isSilent(ops, ins.fbConn, opsNum, isPseudo4ops, moreInfo);
}

bool BanksDump::isSilent(const BanksDump::Operator *ops, uint_fast16_t fbConn, size_t countOps, bool pseudo4op, bool moreInfo)
{
    // TODO: Implement this completely!!!
    const uint_fast8_t conn1 = (fbConn) & 0x01;
    const uint_fast8_t conn2 = (fbConn >> 8) & 0x01;
    OpCheckData opd[4];
    for(size_t i = 0; i < 4; i++)
        opd[i].setData(ops[i].d_E862, ops[i].d_40);

    if(countOps == 2)
    {
        if(conn1 == 0)
        {
            if(opd[1].isOpSilent(moreInfo))
                return true;
        }
        if(conn1 == 1)
        {
            if(opd[0].isOpSilent(moreInfo) && opd[1].isOpSilent(moreInfo))
                return true;
        }
    }
    else if(countOps == 4 && pseudo4op)
    {
        bool silent1 = false;
        bool silent2 = false;
        if(conn1 == 0 && opd[1].isOpSilent(moreInfo))
            silent1 = true;
        if(conn1 == 1 && opd[0].isOpSilent(moreInfo) && opd[1].isOpSilent(moreInfo))
            silent1 = true;
        if(conn2 == 0 && opd[3].isOpSilent(moreInfo))
            silent2 = true;
        if(conn2 == 1 && opd[2].isOpSilent(moreInfo) && opd[3].isOpSilent(moreInfo))
            silent2 = true;
        if(silent1 && silent2)
            return true;
    }
    else if(countOps == 4 && !pseudo4op)
    {
        if(conn1 == 0 && conn2 == 0) // FM-FM [0, 0, 0, 1]
        {
            if(opd[3].isOpSilent(moreInfo))
                return true;
        }

        if(conn1 == 1 && conn2 == 0) // AM-FM [1, 0, 0, 1]
        {
            if(opd[0].isOpSilent(moreInfo) && opd[3].isOpSilent(moreInfo))
                return true;
        }
        if(conn1 == 0 && conn2 == 1) // FM-AM [0, 1, 0, 1]
        {
            if(opd[1].isOpSilent(moreInfo) && opd[3].isOpSilent(moreInfo))
                return true;
        }
        if(conn1 == 1 && conn2 == 1) // FM-AM [1, 0, 1, 1]
        {
            if(opd[0].isOpSilent(moreInfo) && opd[2].isOpSilent(moreInfo) && opd[3].isOpSilent(moreInfo))
                return true;
        }
    }

    return false;
}

void BanksDump::InstrumentEntry::setFbConn(uint_fast16_t fbConn1, uint_fast16_t fbConn2)
{
    fbConn = (static_cast<uint_fast16_t>(fbConn1 & 0x0F)) |
             (static_cast<uint_fast16_t>(fbConn2 & 0x0F) << 8);
}
