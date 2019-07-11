/*
 *  Copyright (C) David C.J. Matthews 2005, 2006
 *     Derived from dsmcc by Richard Palmer
 */
#include <cstdio>
#include <cstdlib>
#include <zlib.h>
#undef Z_NULL
#define Z_NULL nullptr

#include "dsmccobjcarousel.h"
#include "dsmccreceiver.h"
#include "dsmcccache.h"
#include "dsmcc.h"

#include "mythlogging.h"

// Construct the data for a new module from a DII message.
DSMCCCacheModuleData::DSMCCCacheModuleData(DsmccDii *dii,
                                           DsmccModuleInfo *info,
                                           unsigned short streamTag)
    : m_carousel_id(dii->m_download_id), m_module_id(info->m_module_id),
      m_stream_id(streamTag),          m_version(info->m_module_version),
      m_moduleSize(info->m_module_size)
{
    // The number of blocks needed to hold this module.
    int num_blocks = (m_moduleSize + dii->m_block_size - 1) / dii->m_block_size;
    m_blocks.resize(num_blocks, nullptr); // Set it to all zeros

    // Copy the descriptor information.
    m_descriptorData = info->m_modinfo.m_descriptorData;
}

DSMCCCacheModuleData::~DSMCCCacheModuleData()
{
    vector<QByteArray*>::iterator it = m_blocks.begin();
    for (; it != m_blocks.end(); ++it)
        delete *it;
    m_blocks.clear();
}

/** \fn DSMCCCacheModuleData::AddModuleData(DsmccDb*,const unsigned char*)
 *  \brief Add block to the module and create the module if it's now complete.
 *  \return data for the module if it is complete, nullptr otherwise.
 */
unsigned char *DSMCCCacheModuleData::AddModuleData(DsmccDb *ddb,
                                                   const unsigned char *data)
{
    if (m_version != ddb->m_module_version)
    {
        LOG(VB_DSMCC, LOG_WARNING, QString("[dsmcc] Module %1 my version %2 != %3")
            .arg(ddb->m_module_id).arg(m_version).arg(ddb->m_module_version));
        return nullptr; // Wrong version
    }

    if (m_completed)
        return nullptr; // Already got it.

    if (ddb->m_block_number >= m_blocks.size())
    {
        LOG(VB_DSMCC, LOG_WARNING, QString("[dsmcc] Module %1 block number %2 "
                                           "is larger than %3")
            .arg(ddb->m_module_id).arg(ddb->m_block_number)
            .arg(m_blocks.size()));
        return nullptr;
    }

    // Check if we have this block already or not. If not append to list
    if (m_blocks[ddb->m_block_number])
    {
        QString s;
        for (size_t i = 0; i < m_blocks.size(); ++i)
            s += m_blocks[i] ? '+' : 'X';

        LOG(VB_DSMCC, LOG_INFO, QString("[dsmcc] Module %1 block %2 dup: %3")
            .arg(ddb->m_module_id).arg(ddb->m_block_number +1).arg(s));

        return nullptr; // We have seen this block before.
    }

    // Add this to our set of blocks.
    m_blocks[ddb->m_block_number] = new QByteArray((char*) data, ddb->m_len);
    if (m_blocks[ddb->m_block_number])
        m_receivedData += ddb->m_len;

    LOG(VB_DSMCC, LOG_INFO, QString("[dsmcc] Module %1 block %2/%3 bytes %4/%5")
        .arg(ddb->m_module_id)
        .arg(ddb->m_block_number +1).arg(m_blocks.size())
        .arg(m_receivedData).arg(m_moduleSize));

    if (m_receivedData < m_moduleSize)
        return nullptr; // Not yet complete

    LOG(VB_DSMCC, LOG_INFO,
        QString("[dsmcc] Reconstructing module %1 from blocks")
            .arg(m_module_id));

    // Re-assemble the blocks into the complete module.
    unsigned char *tmp_data = (unsigned char*) malloc(m_receivedData);
    if (tmp_data == nullptr)
        return nullptr;

    uint curp = 0;
    for (size_t i = 0; i < m_blocks.size(); i++)
    {
        QByteArray *block = m_blocks[i];
        if (block == nullptr)
        {
            LOG(VB_DSMCC, LOG_INFO,
                QString("[dsmcc] Null data found, aborting reconstruction"));
            free(tmp_data);
            return nullptr;
        }
        m_blocks[i] = nullptr;
        uint size = block->size();
        memcpy(tmp_data + curp, block->data(), size);
        curp += size;
        delete block;
    }

    /* Uncompress....  */
    if (m_descriptorData.m_isCompressed)
    {
        unsigned long dataLen = m_descriptorData.m_originalSize;
        LOG(VB_DSMCC, LOG_INFO, QString("[dsmcc] uncompressing: "
                                        "compressed size %1, final size %2")
            .arg(m_moduleSize).arg(dataLen));

        unsigned char *uncompressed = (unsigned char*) malloc(dataLen);
        int ret = uncompress(uncompressed, &dataLen, tmp_data, m_moduleSize);
        if (ret != Z_OK)
        {
            LOG(VB_DSMCC, LOG_ERR, "[dsmcc] compression error, skipping");
            free(tmp_data);
            free(uncompressed);
            return nullptr;
        }

        free(tmp_data);
        tmp_data = uncompressed;
    }

    m_completed = true;
    m_blocks.clear(); // No longer required: free the space.
    return tmp_data;
}


ObjCarousel::~ObjCarousel()
{
    QLinkedList<DSMCCCacheModuleData*>::iterator it = m_Cache.begin();
    for (; it != m_Cache.end(); ++it)
        delete *it;
    m_Cache.clear();
}

void ObjCarousel::AddModuleInfo(DsmccDii *dii, Dsmcc *status,
                                unsigned short streamTag)
{
    for (int i = 0; i < dii->m_number_modules; i++)
    {
        DsmccModuleInfo *info = &(dii->m_modules[i]);
        bool bFound = false;
        // Do we already know this module?
        // If so and it is the same version we don't need to do anything.
        // If the version has changed we have to replace it.
        QLinkedList<DSMCCCacheModuleData*>::iterator it = m_Cache.begin();
        for ( ; it != m_Cache.end(); ++it)
        {
            DSMCCCacheModuleData *cachep = *it;
            if (cachep->CarouselId() == dii->m_download_id &&
                    cachep->ModuleId() == info->m_module_id)
            {
                /* already known */
                if (cachep->Version() == info->m_module_version)
                {
                    LOG(VB_DSMCC, LOG_DEBUG, QString("[dsmcc] Already Know Module %1")
                            .arg(info->m_module_id));

                    if (cachep->ModuleSize() == info->m_module_size)
                    {
                        bFound = true;
                        break;
                    }
                    // It seems that when ITV4 starts broadcasting it
                    // updates the contents of a file but doesn't
                    // update the version.  This is a work-around.
                    LOG(VB_DSMCC, LOG_ERR,
                        QString("[dsmcc] Module %1 size has changed (%2 to %3) "
                                "but version has not!!")
                             .arg(info->m_module_id)
                             .arg(info->m_module_size)
                             .arg(cachep->DataSize()));
                }
                // Version has changed - Drop old data.
                LOG(VB_DSMCC, LOG_INFO, QString("[dsmcc] Updated Module %1")
                        .arg(info->m_module_id));

                // Remove and delete the cache object.
                m_Cache.erase(it);
                delete cachep;
                break;
            }
        }

        if (bFound)
            continue;

        LOG(VB_DSMCC, LOG_INFO, QString("[dsmcc] Saving info for module %1")
                .arg(dii->m_modules[i].m_module_id));

        // Create a new cache module data object.
        DSMCCCacheModuleData *cachep = new DSMCCCacheModuleData(dii, info, streamTag);
        int tag = info->m_modinfo.m_tap.m_assoc_tag;
        LOG(VB_DSMCC, LOG_DEBUG, QString("[dsmcc] Module info tap identifies "
                                         "tag %1 with carousel %2")
            .arg(tag).arg(cachep->CarouselId()));

        // If a carousel with this id does not exist create it.
        status->AddTap(tag, cachep->CarouselId());

        // Add this module to the cache.
        m_Cache.append(cachep);
    }
}

/** \fn ObjCarousel::AddModuleData(unsigned long,DsmccDb*,const unsigned char*)
 *  \brief We have received a block for a module.
 *
 *   Add it to the module and process the module if it's now complete.
 */
void ObjCarousel::AddModuleData(DsmccDb *ddb, const unsigned char *data)
{
    LOG(VB_DSMCC, LOG_DEBUG, QString("[dsmcc] Data block on carousel %1").arg(m_id));

    // Search the saved module info for this module
    QLinkedList<DSMCCCacheModuleData*>::iterator it = m_Cache.begin();
    for (; it != m_Cache.end(); ++it)
    {
        DSMCCCacheModuleData *cachep = *it;
        if (cachep->CarouselId() == m_id &&
            (cachep->ModuleId() == ddb->m_module_id))
        {
            // Add the block to the module
            unsigned char *tmp_data = cachep->AddModuleData(ddb, data);
            if (tmp_data)
            {
                // It is complete and we have the data
                unsigned int len   = cachep->DataSize();
                unsigned long curp = 0;
                LOG(VB_DSMCC, LOG_DEBUG, QString("[biop] Module size (uncompressed) = %1").arg(len));

                // Now process the BIOP tables in this module.
                // Tables may be file contents or the descriptions of
                // directories or service gateways (root directories).
                while (curp < len)
                {
                    BiopMessage bm;
                    if (!bm.Process(cachep, &m_filecache, tmp_data, &curp))
                        break;
                }
                free(tmp_data);
            }
            return;
        }
    }
    LOG(VB_DSMCC, LOG_INFO, QString("[dsmcc] Data block module %1 not on carousel %2")
        .arg(ddb->m_module_id).arg(m_id));
}
