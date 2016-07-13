#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <malloc.h>

#include <typeinfo>
#include <cmath>
#include <numeric>
#include <iterator>
#include <algorithm>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <3ds.h>

#include "config.h"
#include "menu.h"
#include "utils.h"
#include "cia.h"
#include "data.h"
#include "types.h"

#include "svchax/svchax.h"
#include "json/json.h"
#include "fts_fuzzy_match.h"
#include "utf8proc/utf8proc.h"

static const u16 top = 0x140;
static bool bSvcHaxAvailable = true;
static bool bExit = false;
static bool bRedoSearch = false;
int sourceDataType;
Json::Value sourceData;

CConfig config;

struct find_game_item {
    std::string titleid;
    find_game_item(std::string titleid) : titleid(titleid) {}
    bool operator () ( const game_item& gi ) const {
        return gi.titleid == titleid;
    }
};

// Vector used for download queue
std::vector<game_item> game_queue;

bool compareByScore(const game_item &a, const game_item &b)
{
    return a.score > b.score;
}

Result ProcessCIA(std::string dir, std::string titleName)
{
    FILE *tik = fopen((dir + "/ticket").c_str(), "rb");
    if (!tik) 
    {
        return -1;
    }
    TIK_CONTEXT tik_context = process_tik(tik);

    FILE *tmd = fopen((dir + "/tmd").c_str(),"rb");
    if (!tmd) 
    {
        fclose(tik);
        return -1;
    }
    TMD_CONTEXT tmd_context = process_tmd(tmd);

    if(tik_context.result != 0 || tmd_context.result != 0){
        printf("[!] Input files could not be processed successfully\n");
        free(tmd_context.content_struct);
        fclose(tik);
        fclose(tmd);
        return -1;
    }

    int result;
    if (config.GetMode() == CConfig::Mode::INSTALL_CIA)
    {
        result = install_cia(tmd_context, tik_context);
    }
    else
    {
        FILE *output = fopen((dir + "/" + titleName + ".cia").c_str(),"wb");
        if (!output) 
        {
            result = -2;
        }
        else
        {
            result = generate_cia(tmd_context, tik_context, output);
            if(result != 0)
            {
                remove((dir + "/" + titleName + ".cia").c_str());
            }
        }
    }

    // free allocated memory/handles
    free(tmd_context.content_struct);
    fclose(tik);
    fclose(tmd);

    // Clean up temp files
    remove((dir + "/ticket").c_str());
    remove((dir + "/tmd").c_str());
    return result;
}

std::string u32_to_hex_string(u32 i)
{
    std::stringstream stream;
    stream << std::setfill ('0') << std::setw(sizeof(u32)*2) << std::hex << i;
    return stream.str();
}

int mkpath(std::string s,mode_t mode)
{
    size_t pre=0,pos;
    std::string dir;
    int mdret = 0;

    if(s[s.size()-1]!='/'){
        // force trailing / so we can handle everything in loop
        s+='/';
    }

    while((pos=s.find_first_of('/',pre))!=std::string::npos){
        dir=s.substr(0,pos++);
        pre=pos;
        if(dir.size()==0) continue; // if leading / first time is 0 length
        if((mdret=mkdir(dir.c_str(),mode)) && errno!=EEXIST){
            return mdret;
        }
    }
    return mdret;
}

char parse_hex(char c)
{
    if ('0' <= c && c <= '9') return c - '0';
    if ('A' <= c && c <= 'F') return c - 'A' + 10;
    if ('a' <= c && c <= 'f') return c - 'a' + 10;
    std::abort();
}

char* parse_string(const std::string & s)
{
    char* buffer = new char[s.size() / 2];
    for (std::size_t i = 0; i != s.size() / 2; ++i)
        buffer[i] = 16 * parse_hex(s[2 * i]) + parse_hex(s[2 * i + 1]);
    return buffer;
}

std::string get_file_contents(const char *filename)
{
  std::ifstream in(filename, std::ios::in | std::ios::binary);
  if (in)
  {
    return(std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>()));
  }
  throw(errno);
}

void CreateTicket(std::string titleId, std::string encTitleKey, char* titleVersion, std::string outputFullPath)
{
    std::ofstream ofs;

    ofs.open(outputFullPath, std::ofstream::out | std::ofstream::binary | std::ofstream::trunc);
    ofs.write(tikTemp, TICKET_SIZE);
    ofs.close();

    ofs.open(outputFullPath, std::ofstream::out | std::ofstream::in | std::ofstream::binary);

    //write version
    ofs.seekp(top+0xA6, std::ios::beg);
    ofs.write(titleVersion, 0x2);

    //write title id
    char* nTitleID = parse_string(titleId);
    ofs.seekp(top+0x9C, std::ios::beg);
    ofs.write(nTitleID, 0x8);
    free(nTitleID);

    //write key
    char* nTitleKey = parse_string(encTitleKey);
    ofs.seekp(top+0x7F, std::ios::beg);
    ofs.write(nTitleKey, 0x10);
    free(nTitleKey);

    ofs.close();
}

void InstallTicket(std::string FullPath, std::string TitleId)
{
    Handle hTik;
    u32 writtenbyte;
    std::string curr = get_file_contents(FullPath.c_str());

    // Remove the ticket incase there was a bad one previously installed
    char* nTitleId = parse_string(TitleId);
    u64 titleId = u8_to_u64((u8*)nTitleId, BIG_ENDIAN);
    free (nTitleId);
    AM_DeleteTicket(titleId);

    // Install new ticket
    AM_InstallTicketBegin(&hTik);
    FSFILE_Write(hTik, &writtenbyte, 0, curr.c_str(), 0x100000, 0);
    AM_InstallTicketFinish(hTik);
    printf("Ticket Installed.");
    //delete temp ticket, ticket folder still exists... ugly. later stream directly to the handle
    remove(FullPath.c_str());
}

Result DownloadTitle(std::string titleId, std::string encTitleKey, std::string titleName, std::string region)
{
    // Convert the titleid to a u64 for later use
    char* nTitleId = parse_string(titleId);
    u64 uTitleId = u8_to_u64((u8*)nTitleId, BIG_ENDIAN);
    free (nTitleId);

    // Wait for wifi to be available
    u32 wifi = 0;
    Result ret = 0;
    Result res = 0;
    while(R_SUCCEEDED(ret = ACU_GetWifiStatus(&wifi)) && wifi == 0)
    {
        hidScanInput();
        if (hidKeysDown() & KEY_B)
        {
            ret = -1;
            break;
        }
    }

    if (R_FAILED(ret))
    {
        printf("Unable to access internet.\n");
        return ret;
    }

    std::string outputDir = "/CIAngel";

    if (titleName.length() == 0)
    {
        titleName = titleId;
    }

    // Include region in filename
    if (region.length() > 0)
    {
        titleName = titleName + " (" + region + ")";
    }

    std::string mode_text;
    if(config.GetMode() == CConfig::Mode::DOWNLOAD_CIA)
    {
        mode_text = "create";
    }
    else if(config.GetMode() == CConfig::Mode::INSTALL_CIA)
    {
        mode_text = "install";
    }


    printf("Starting - %s\n", titleName.c_str());

    // If in install mode, download/install the SEED entry
    if (config.GetMode() == CConfig::Mode::INSTALL_CIA)
    {
        // Download and install the SEEDDB entry if install mode
        // Code based on code from FBI: https://github.com/Steveice10/FBI/blob/master/source/core/util.c#L254
        // Copyright (C) 2015 Steveice10
        u8 seed[16];
        static const char* regionStrings[] = {"JP", "US", "GB", "GB", "HK", "KR", "TW"};
        u8 region = CFG_REGION_USA;
        CFGU_GetSystemLanguage(&region);

        if(region <= CFG_REGION_TWN) {
            char url[128];
            snprintf(url, 128, SEED_URL "0x%016llX/ext_key?country=%s", uTitleId, regionStrings[region]);

            httpcContext context;
            if(R_SUCCEEDED(res = httpcOpenContext(&context, HTTPC_METHOD_GET, url, 1))) {
                httpcSetSSLOpt(&context, SSLCOPT_DisableVerify);

                u32 responseCode = 0;
                if(R_SUCCEEDED(res = httpcBeginRequest(&context)) && R_SUCCEEDED(res = httpcGetResponseStatusCode(&context, &responseCode, 0))) {
                    if(responseCode == 200) {
                        u32 pos = 0;
                        u32 bytesRead = 0;
                        while(pos < sizeof(seed) && (R_SUCCEEDED(res = httpcDownloadData(&context, &seed[pos], sizeof(seed) - pos, &bytesRead)) || (u32)res == HTTPC_RESULTCODE_DOWNLOADPENDING)) {
                            pos += bytesRead;
                        }
                    } else {
                        res = -1;
                    }
                }

                httpcCloseContext(&context);
            }

            if (R_SUCCEEDED(res))
            {
                res = InstallSeed(uTitleId, seed);
                if (R_FAILED(res))
                {
                    printf("Error installing SEEDDB entry: 0x%lx\n", res);
                }
            }
        }
    }

    // Make sure the CIA doesn't already exist
    std::string cp = outputDir + "/" + titleName + ".cia";
    if (config.GetMode() == CConfig::Mode::DOWNLOAD_CIA && FileExists(cp.c_str()))
    {
        printf("%s already exists.\n", cp.c_str());
        return 0;
    }

    std::ofstream ofs;

    FILE *oh = fopen((outputDir + "/tmp/tmd").c_str(), "wb");
    if (!oh) 
    {
        printf("Error opening %s/tmp/tmd\n", outputDir.c_str());
        return -1;
    }
    res = DownloadFile((NUS_URL + titleId + "/tmd").c_str(), oh, false);
    fclose(oh);
    if (res != 0)
    {
        printf("Could not download TMD. Internet/Title ID is OK?\n");
        return res;
    }

    // Read version
    std::ifstream tmdfs;
    tmdfs.open(outputDir + "/tmp/tmd", std::ofstream::out | std::ofstream::in | std::ofstream::binary);
    char titleVersion[2];
    tmdfs.seekg(top+0x9C, std::ios::beg);
    tmdfs.read(titleVersion, 0x2);
    tmdfs.close();

    CreateTicket(titleId, encTitleKey, titleVersion, outputDir + "/tmp/ticket");

    printf("Now %s the CIA...\n", mode_text.c_str());

    res = ProcessCIA(outputDir + "/tmp", titleName);
    if (res != 0)
    {
        printf("Could not %s the CIA.\n", mode_text.c_str());
        return res;
    }

    if (config.GetMode() == CConfig::Mode::DOWNLOAD_CIA)
    {
        rename((outputDir + "/tmp/" + titleName + ".cia").c_str(), (outputDir + "/" + titleName + ".cia").c_str());
    }

    printf(" DONE!\n");

    return res;
}

void ProcessGameQueue()
{
    // Create the tickets folder if we're in ticket mode
    char empty_titleVersion[2] = {0x00, 0x00};

    std::vector<game_item>::iterator game = game_queue.begin();
    while(aptMainLoop() && game != game_queue.end())
    {
        std::string selected_titleid = (*game).titleid;
        std::string selected_enckey = (*game).titlekey;
        std::string selected_name = (*game).name;
        std::string selected_region = (*game).region;

        if (config.GetMode() == CConfig::Mode::INSTALL_TICKET)
        {
            CreateTicket(selected_titleid, selected_enckey, empty_titleVersion, "/CIAngel/tmp/ticket");
            InstallTicket("/CIAngel/tmp/ticket", selected_titleid);
        }
        else
        {
            Result res = DownloadTitle(selected_titleid, selected_enckey, selected_name, selected_region);
            if (R_FAILED(res)) {
                printf("Error processing queue. Returning to menu\n");
                break;
            }
        }

        game = game_queue.erase(game);
    }

    wait_key_specific("Press A to continue.\n", KEY_A);
}

void removeForbiddenChar(std::string* s)
{
    std::string::iterator it;
    std::string illegalChars = "\\/:?\"<>|";
    for (it = s->begin() ; it < s->end() ; ++it){
        bool found = illegalChars.find(*it) != std::string::npos;
        if(found)
        {
            *it = ' ';
        }
    }
}

std::istream& GetLine(std::istream& is, std::string& t)
{
    t.clear();
    std::istream::sentry se(is, true);
    std::streambuf* sb = is.rdbuf();

    for (;;) {
        int c = sb->sbumpc();
        switch (c) {
            case '\n':
              return is;
            case '\r':
              if (sb->sgetc() == '\n')
                sb->sbumpc();
              return is;
            case  EOF:
              if (t.empty())
                is.setstate(std::ios::eofbit);
              return is;
            default:
              t += (char)c;
        }
    }
}

std::string ToHex(const std::string& s)
{
    std::ostringstream ret;
    for (std::string::size_type i = 0; i < s.length(); ++i)
    {
        int z = s[i]&0xff;
        ret << std::hex << std::setfill('0') << std::setw(2) << z;
    }
    return ret.str();
}

void load_JSON_data() 
{
    printf("loading wings.json...\n");
    std::ifstream ifs("/CIAngel/wings.json");
    Json::Reader reader;
    Json::Value obj;
    reader.parse(ifs, obj);
    sourceData = obj; // array of characters
    
    if(sourceData[0]["titleID"].isString()) {
      sourceDataType = JSON_TYPE_ONLINE;
    } else if (sourceData[0]["titleid"].isString()) {
      sourceDataType = JSON_TYPE_WINGS;
    }
}

void loadConfig()
{
    // Load config, and force mode to DOWNLOAD_CIA if svcHax not available, then resave
    config.LoadConfig("/CIAngel/config.json");
    if (!bSvcHaxAvailable)
    {
        config.SetMode(CConfig::Mode::DOWNLOAD_CIA);
    }
    config.SaveConfig();
}

// Search menu keypress callback
bool menu_search_keypress(int selected, u32 key, void* data)
{
    std::vector<game_item>* cb_data = (std::vector<game_item>*)data;

    // If key is 0, it means aptMainLoop() returned false, so we're exiting
    // Go back to the previous menu which will handle quitting
    if (!key) {
        return true;
    }

    // B goes back a screen
    if (key & KEY_B)
    {
        return true;
    }

    // A triggers the default action on the selected title
    if (key & KEY_A)
    {
        // Clean up the console since we'll be using it
        consoleClear();

        // Fetch the title data and start downloading
        std::string selected_titleid = (*cb_data)[selected].titleid;
        std::string selected_enckey = (*cb_data)[selected].titlekey;
        std::string selected_name = (*cb_data)[selected].name;
        std::string selected_region = (*cb_data)[selected].region;

        printf("OK - %s\n", selected_name.c_str());
        //removes any problem chars, not sure if whitespace is a problem too...?
        removeForbiddenChar(&selected_name);

        if(config.GetMode() == CConfig::Mode::INSTALL_TICKET)
        {
            char empty_titleVersion[2] = {0x00, 0x00};
            CreateTicket(selected_titleid, selected_enckey, empty_titleVersion, "/CIAngel/tmp/ticket");
            InstallTicket("/CIAngel/tmp/ticket", selected_titleid);
        }
        else
        {
            DownloadTitle(selected_titleid, selected_enckey, selected_name, selected_region);
        }

        wait_key_specific("\nPress A to continue.\n", KEY_A);
        return true;
    }

    // X triggers adding items to the download queue
    if (key & KEY_X)
    {
        consoleClear();
        std::string titleid = (*cb_data)[selected].titleid;
        if (std::find_if(game_queue.begin(), game_queue.end(), find_game_item(titleid)) == game_queue.end())
        {
            game_queue.push_back((*cb_data)[selected]);

            printf("Game added to queue.\n");
        }
        else
        {
            printf("Game already in queue.\n");
        }

        printf("Queue size: %d\n", game_queue.size());

        if(wait_key_specific("\nPress A to continue.\nPress B to go back to the list.\n", KEY_A | KEY_B) & KEY_B)
        {
            bRedoSearch = true;
        }
        return true;
    }

    return false;
}

/* Search filter functions */
// Fuzzy match based on the game name
bool search_by_name(std::string &searchString, Json::Value &gameData, int &outScore)
{
    return fts::fuzzy_match(searchString.c_str(), gameData["ascii_name"].asCString(), outScore);
}

// Wildcard match based on game serial
bool search_by_serial(std::string &searchString, Json::Value &gameData, int &outScore)
{
    if (sourceDataType == JSON_TYPE_WINGS) 
    {
        return (upperCase(gameData["code"].asString()).find(upperCase(searchString)) != std::string::npos);
    }
    else
    {
        return (upperCase(gameData["serial"].asString()).find(upperCase(searchString)) != std::string::npos);
    }
}

/* Menu Action Functions */
void action_search(bool (*match)(std::string &searchString, Json::Value &gameData, int &outScore))
{
    SwkbdState swkbd;

    consoleClear();
    printf("Please enter text to search for:\n");
		
		swkbdInit(&swkbd, SWKBD_TYPE_NORMAL, 2, -1);
		swkbdSetValidation(&swkbd, SWKBD_NOTEMPTY_NOTBLANK, 0, 0);
		swkbdSetHintText(&swkbd, "Please enter text to search for");
		char textBuf[255];
		if (swkbdInputText(&swkbd, textBuf, sizeof(textBuf)) != SWKBD_BUTTON_CONFIRM)
    {
        return;
    }
    std::string searchString(textBuf);
    
    do
    {
    
    bRedoSearch = false;

    // User has entered their input, so let's scrap the keyboard
    clear_screen(GFX_BOTTOM);

    std::vector<game_item> display_output;
    int outScore;
    
    for (unsigned int i = 0; i < sourceData.size(); i++) {
        // Check the region filter
        std::string regionFilter = config.GetRegionFilter();
        if(regionFilter != "off" && sourceData[i]["region"].asString() != regionFilter) {
            continue;
        }

        // Check that the encTitleKey isn't null
        if (sourceData[i]["encTitleKey"].isNull())
        {
            continue;
        }

        // Create an ASCII version of the name if one doesn't exist yet
        if (sourceData[i]["ascii_name"].isNull())
        {
            // Normalize the name down to ASCII
            utf8proc_option_t options = (utf8proc_option_t)(UTF8PROC_NULLTERM | UTF8PROC_STABLE | UTF8PROC_DECOMPOSE | UTF8PROC_COMPAT | UTF8PROC_STRIPMARK | UTF8PROC_STRIPCC);
            utf8proc_uint8_t* szName;
            utf8proc_uint8_t *str = (utf8proc_uint8_t*)sourceData[i]["name"].asCString();
            utf8proc_map(str, 0, &szName, options);

            sourceData[i]["ascii_name"] = (const char*)szName;

            free(szName);
        }

        if (match(searchString, sourceData[i], outScore))
        {

            game_item item;
            item.score = outScore;
            item.index = i;

            switch(sourceDataType) {
            case JSON_TYPE_WINGS:
              item.titleid = sourceData[i]["titleid"].asString();
              item.titlekey = sourceData[i]["enckey"].asString();
              item.name = sourceData[i]["ascii_name"].asString();
              item.region = sourceData[i]["region"].asString();
              item.code = sourceData[i]["code"].asString();
              break;
            case JSON_TYPE_ONLINE:
              item.titleid = sourceData[i]["titleID"].asString();
              item.titlekey = sourceData[i]["encTitleKey"].asString();
              item.name = sourceData[i]["ascii_name"].asString();
              item.region = sourceData[i]["region"].asString();
              item.code = sourceData[i]["serial"].asString();
              break;
            }

            std::string typeCheck = item.titleid.substr(4,4);
            //if title id belongs to gameapp/dlc/update/dsiware, use it. if not, ignore. case sensitve of course
            if(typeCheck == "0000" || typeCheck == "008c" || typeCheck == "000e" || typeCheck == "8004"){
                display_output.push_back(item);
            }
        }
    }

    unsigned int display_amount = display_output.size();

    // We technically have 30 rows to work with, minus 2 for header/footer. But stick with 20 entries for now

    if (display_amount == 0)
    {
        printf("No matching titles found.\n");
        wait_key_specific("\nPress A to return.\n", KEY_A);
        return;
    }

    // sort similar names by fuzzy score
    if(display_amount>1) {
        std::sort(display_output.begin(), display_output.end(), compareByScore);
    }
    
    std::string mode_text;
    switch (config.GetMode())
    {
        case CConfig::Mode::DOWNLOAD_CIA:
            mode_text = "Create CIA";
        break;
        case CConfig::Mode::INSTALL_CIA:
            mode_text = "Install CIA";
        break;
        case CConfig::Mode::INSTALL_TICKET:
            mode_text = "Create Ticket";
        break;
    }

    char footer[51];
    char header[51];
    sprintf(header, "Select a Title (found %i results)", display_amount);
    sprintf(footer, "Press A to %s. Press X to queue.", mode_text.c_str());
    titles_multkey_draw(header, footer, 1, &display_output, &display_output, menu_search_keypress);
    
    } while(bRedoSearch == true);
}

void action_prompt_queue()
{
    consoleClear();

    std::string mode_text;
    switch (config.GetMode())
    {
        case CConfig::Mode::DOWNLOAD_CIA:
            mode_text = "download";
        break;
        case CConfig::Mode::INSTALL_CIA:
            mode_text = "install";
        break;
        case CConfig::Mode::INSTALL_TICKET:
            mode_text = "create tickets for";
        break;
    }

    printf("Queue contains %d items.\n", game_queue.size());
    printf("Press A to %s queue.\n", mode_text.c_str());
    printf("Press B to return to menu.\n");
    printf("Press X to clear queue.\n");

    while (aptMainLoop())
    {
        u32 key = wait_key();
        if (key & KEY_B)
        {
            break;
        }

        if (key & KEY_X)
        {
            game_queue.clear();
            break;
        }

        if (key & KEY_A)
        {
            ProcessGameQueue();
            break;
        }
    }

}

void action_manual_entry()
{
    SwkbdState swkbd;
    swkbdInit(&swkbd, SWKBD_TYPE_NORMAL, 2, -1);
    swkbdSetValidation(&swkbd, SWKBD_NOTEMPTY_NOTBLANK, 0, 0);
    char textBuf[255];
    if (swkbdInputText(&swkbd, textBuf, sizeof(textBuf)) != SWKBD_BUTTON_CONFIRM)
    {
        return;
    }

    consoleClear();

    // Keep looping so the user can retry if they enter a bad id/key
    while(true)
    {
        printf("Please enter a titleID:\n");
        swkbdSetHintText(&swkbd, "Please enter a titleID");
        if (swkbdInputText(&swkbd, textBuf, sizeof(textBuf)) != SWKBD_BUTTON_CONFIRM)
        {
            break;
        }
        std::string titleId(textBuf);
        std::string key;

        for (unsigned int i = 0; i < sourceData.size(); i++){
            std::string tempId = sourceData[i]["titleid"].asString();
            std::string tempKey = sourceData[i]["enckey"].asString();

            if(tempId.compare(titleId) == 0 && tempKey.length() == 32) {
               printf("Found encTitleKey, proceeding automatically\n"); 
               key = tempKey;
               break;
            }
        }
        if(key.length() != 32) {
            printf("Please enter the corresponding encTitleKey:\n");
            swkbdSetHintText(&swkbd, "Please enter the corresponding encTitleKey");
            if (swkbdInputText(&swkbd, textBuf, sizeof(textBuf)) != SWKBD_BUTTON_CONFIRM)
            {
                break;
            }
            key.assign(textBuf, sizeof(textBuf));
        }
        if (titleId.length() == 16 && key.length() == 32)
        {
            DownloadTitle(titleId, key, "", "");
            wait_key_specific("\nPress A to continue.\n", KEY_A);
            break;
        }
        else
        {   
            printf("There was an error in you input:\n");  
            if(titleId.length() != 16) {
                printf("titleIDs are 16 chars long, not %i\n", titleId.length());
            }
            if(key.length() != 32) {
                printf("encTitleKeys are 32 chars long, not %i\n", key.length());
            }
        }
    }
}

void action_input_txt()
{
    consoleClear();

    std::ifstream input;
    std::string titleId;
    std::string key;

    input.open("/CIAngel/input.txt", std::ofstream::in);
    GetLine(input, titleId);
    GetLine(input, key);
    DownloadTitle(titleId, key, "", "");

    wait_key_specific("\nPress A to continue.\n", KEY_A);
}

void action_toggle_install()
{
    consoleClear();
    CConfig::Mode nextMode = CConfig::Mode::INSTALL_CIA;

    switch (config.GetMode())
    {
        case CConfig::Mode::DOWNLOAD_CIA:
            nextMode = CConfig::Mode::INSTALL_CIA;
        break;
        case CConfig::Mode::INSTALL_CIA:
            nextMode = CConfig::Mode::INSTALL_TICKET;
        break;
        case CConfig::Mode::INSTALL_TICKET:
            nextMode = CConfig::Mode::DOWNLOAD_CIA;
        break;
    }
    
    if (nextMode == CConfig::Mode::INSTALL_TICKET || nextMode == CConfig::Mode::INSTALL_CIA)
    {
        if (!bSvcHaxAvailable)
        {
            nextMode = CConfig::Mode::DOWNLOAD_CIA;
            printf(CONSOLE_RED "Kernel access not available.\nCan't enable Install modes.\nYou can only make a CIA.\n" CONSOLE_RESET);
            wait_key_specific("\nPress A to continue.", KEY_A);
        }
    }

    config.SetMode(nextMode);
}

void action_toggle_region()
{
    consoleClear();
    std::string regionFilter = config.GetRegionFilter();
    if(regionFilter == "off") {
        regionFilter = "ALL";
    } else if (regionFilter == "ALL") {
        regionFilter = "EUR";
    } else if (regionFilter == "EUR") {
        regionFilter = "USA";
    } else if (regionFilter == "USA") {
        regionFilter = "JPN";
    } else if (regionFilter == "JPN") {
        regionFilter = "off";
    }
    config.SetRegionFilter(regionFilter);
}

void action_about()
{
    consoleClear();

    printf(CONSOLE_RED "CIAngel\n\n" CONSOLE_RESET);
    printf("Download, create, and install CIAs directly\n");
    printf("from Nintendo's CDN servers. Grabbing the\n");
    printf("latest games has never been so easy.\n\n");

    printf("Contributors: Cearp, Drakia, superbudvar,\n");
    printf("              mysamdog, cerea1killer,\n");
    printf("              DanTheMan827\n");

    printf("\n\nCommit: " REVISION_STRING "\n\n");

    printf("\nPress any button to continue.");
    wait_key();
}

void action_exit()
{
    bExit = true;
}

void action_download_json()
{
    consoleClear();

    download_JSON();
    load_JSON_data();
}

// Main menu keypress callback
bool menu_main_keypress(int selected, u32 key, void*)
{
    // If key is 0, it means aptMainLoop() returned false, so we're quitting
    if (!key) {
        return true;
    }

    // A button triggers standard actions
    if (key & KEY_A)
    {
        switch (selected)
        {
            case 0:
                action_search(search_by_name);
            break;
            case 1:
                action_search(search_by_serial);
            break;
            case 2:
                action_prompt_queue();
            break;
            case 3:
                action_manual_entry();
            break;
            case 4:
                action_input_txt();
            break;
            case 5:
                action_download_json();
            break;
            case 6:
                action_about();
            break;
            case 7:
                action_exit();
            break;
        }
        return true;
    }
    // L button triggers mode toggle
    else if (key & KEY_L)
    {
        action_toggle_install();
        return true;
    }
    // R button triggers region toggle
    else if (key & KEY_R)
    {
        action_toggle_region();
        return true;
    }

    return false;
}

// Draw the main menu
void menu_main()
{
    const char *options[] = {
        "Search for a title by name",
        "Search for a title by serial",
        "Process download queue",
        "Enter a title key/ID pair",
        "Fetch title key/ID from input.txt",
        "Download wings.json",
        "About CIAngel",
        "Exit",
    };
    char footer[50];

    while (!bExit && aptMainLoop())
    {
        std::string mode_text;
        switch (config.GetMode())
        {
            case CConfig::Mode::DOWNLOAD_CIA:
                mode_text = "Create CIA";
            break;
            case CConfig::Mode::INSTALL_CIA:
                mode_text = "Install CIA";
            break;
            case CConfig::Mode::INSTALL_TICKET:
                mode_text = "Create Ticket";
            break;
        }

        // We have to update the footer every draw, incase the user switches install mode or region
        sprintf(footer, "Mode (L):%s Region (R):%s Queue: %d", mode_text.c_str(), config.GetRegionFilter().c_str(), game_queue.size());

        menu_multkey_draw("CIAngel by cearp and Drakia", footer, 0, sizeof(options) / sizeof(char*), options, NULL, menu_main_keypress);

        clear_screen(GFX_BOTTOM);
    }
}

int main(int argc, const char* argv[])
{
    /* Sadly svchax crashes too much, so only allow install mode when running as a CIA
    // Trigger svchax so we can install CIAs
    if(argc > 0) {
        svchax_init(true);
        if(!__ctr_svchax || !__ctr_svchax_srv) {
            bSvcHaxAvailable = false;
            //printf("Failed to acquire kernel access. Install mode disabled.\n");
        }
    }
    */
    
    // argc is 0 when running as a CIA, and 1 when running as a 3dsx
    if (argc > 0)
    {
        bSvcHaxAvailable = false;
    }

    u32 *soc_sharedmem, soc_sharedmem_size = 0x100000;
    gfxInitDefault();
    consoleInit(GFX_TOP, NULL);

    httpcInit(0);
    soc_sharedmem = (u32 *)memalign(0x1000, soc_sharedmem_size);
    socInit(soc_sharedmem, soc_sharedmem_size);
    sslcInit(0);
    hidInit();
    acInit();
    cfguInit();

    if (bSvcHaxAvailable)
    {
        amInit();
        AM_InitializeExternalTitleDatabase(false);
    }

    init_menu(GFX_TOP);

    // Make sure all CIAngel directories exists on the SD card
    mkpath("/CIAngel", 0777);
    mkpath("/CIAngel/tmp/", 0777);
    loadConfig();
    
    // Set up the reading of json
    check_JSON();
    load_JSON_data();
    
    menu_main();

    if (bSvcHaxAvailable)
    {
        amExit();
    }

    cfguExit();
    acExit();
    gfxExit();
    hidExit();
    httpcExit();
    socExit();
    sslcExit();
}
