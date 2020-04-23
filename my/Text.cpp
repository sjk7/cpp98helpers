   // put the WHOLE header in a bitstream
    CBitStream bs;
    DWORD cbits = 0;
    BYTE* pbits = bs.BytesToBits(pbuf, (DWORD)8, cbits);
    if (!pbits) return;
    // 120 bytes is the MAX size Xing will ever be
    // but we need to get the first 8 bytes, in order to get the flags to work
    // out how big we really are:
    int flags = my::numbers::binary::BinaryToDecimal(&pbits[32], 32, 0, 32);
    m_XingHeader.flags = flags;

    // how big are we now we know the flags??
    int size = 8;
    // ASSERT("THESE WERE ALL ORs" == 0);
    if (flags & 1) size += 4; // frames field

    if (flags & 2) size += 4; // bytes field

    if (flags & 4) // TOC field
        size += 100;

    if (flags & 8) size += 4; // quality

    if (cb < size) {
        bNeedMoreData = TRUE;
        return;
    }

    // now we can get ALL the bits making up this header:
    pbits = bs.BytesToBits(pbuf, (DWORD)size, cbits);
    if (!pbits) return;

    ASSERT(cbits <= 120 * 8);
    int bitpos = 8 * BITS_IN_BYTE;

    if (flags & 1) {
        // we have frames field!

        m_XingHeader.numframes
            = my::numbers::binary::BinaryToDecimal(&pbits[bitpos], 32, 0, 32);

        bitpos += 4 * BITS_IN_BYTE;
    }

    if (flags & 2) {
        // we have bytes field:
        //(and presumably these are audio bytes)
        m_XingHeader.numbytes
            = my::numbers::binary::BinaryToDecimal(&pbits[bitpos], 32, 0, 32);
        bitpos += 4 * BITS_IN_BYTE;
    }

    if (flags & 4) {
        //// we have seeking TOC:
        size_t pos = bitpos / BITS_IN_BYTE;
        memcpy(&m_XingHeader.toc[0], &pbuf[pos], 100);
        bitpos += 100 * BITS_IN_BYTE;
    }

    if (flags & 8) {
        // we have quality indicator:
        m_XingHeader.quality
            = my::numbers::binary::BinaryToDecimal(&pbits[bitpos], 32, 0, 32);
        bitpos += 4 * BITS_IN_BYTE;
    }

    // we are only valid if what the Xing header claims can actually *fit* in
    // the file!
    if (m_XingHeader.numbytes > filesizeinbytes) {
        m_XingHeader.valid = FALSE;
        return;
    }

    m_XingHeader.valid = TRUE;
