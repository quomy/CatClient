#include "catclient.h"

#include <engine/gfx/image_loader.h>
#include <base/log.h>
#include <base/system.h>
#include <base/time.h>

#include <engine/client.h>
#include <engine/config.h>
#include <engine/console.h>
#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/storage.h>

#include <generated/client_data.h>
#include <generated/protocol.h>

#if defined(CONF_VIDEORECORDER)
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/error.h>
#include <libavutil/mathematics.h>
#include <libswscale/swscale.h>
};

struct CAvByteBufferReader
{
	const uint8_t *m_pData = nullptr;
	size_t m_Size = 0;
	size_t m_ReadOffset = 0;
};
#endif

#include <game/client/gameclient.h>
#include <game/teamscore.h>

#include <cerrno>
#include <algorithm>
#include <chrono>

#include "modules/catclient/catclient_media_core.h"
#include "modules/catclient/catclient_state.h"
#include "modules/catclient/catclient_arrows.h"
#include "modules/catclient/catclient_cursor.h"
#include "modules/catclient/catclient_background.h"
#include "modules/catclient/catclient_lifecycle.h"
#include "modules/catclient/catclient_social.h"
