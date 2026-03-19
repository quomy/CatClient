#include "catclient_nametags.h"
#include <base/system.h>
#include <base/time.h>
#include <engine/client.h>
#include <engine/shared/json.h>
#include <engine/shared/jsonwriter.h>
#include <engine/storage.h>
#include <game/client/gameclient.h>
#include <chrono>
#include <string>

#include "modules/nametags/catclient_nametags_core.h"
#include "modules/nametags/catclient_nametags_requests.h"
#include "modules/nametags/catclient_nametags_icon.h"
#include "modules/nametags/catclient_nametags_lifecycle.h"
