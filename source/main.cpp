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
#include <hbkb.h>

#include <3ds.h>

#include "menu.h"
#include "utils.h"
#include "cia.h"
#include "data.h"

#include "svchax/svchax.h"
#include "json/json.h"

static const u16 top = 0x140;
static bool bSvcHaxAvailable = true;
static bool bExit = false;
Json::Value sourceData;
enum install_modes {make_cia, install_direct, install_ticket};
install_modes selected_mode = make_cia;

static std::string regionFilter = "off";

std::string upper(std::string s)
{
  std::string ups;
  
  for(unsigned int i = 0; i < s.size(); i++)
  {
    ups.push_back(std::toupper(s[i]));
  }
  
  return ups;
}


struct find_game_item {
    std::string titleid;
    find_game_item(std::string titleid) : titleid(titleid) {}
    bool operator () ( const game_item& gi ) const {
        return gi.titleid == titleid;
    }
};

// Vector used for download queue
std::vector<game_item> game_queue;

bool compareByLD(const game_item &a, const game_item &b)
{
    return a.ld < b.ld;
}

Result ConvertToCIA(std::string dir, std::string titleName)
{
    char cwd[1024];
    if (getcwdir(cwd, sizeof(cwd)) == NULL){
        printf("[!] Could not store Current Working Directory\n");
        return -1;
    }
    chdir(dir.c_str());
    FILE *tik = fopen("cetk", "rb");
    if (!tik) return -1;
    TIK_CONTEXT tik_context = process_tik(tik);

    FILE *tmd = fopen((dir + "/tmd").c_str(),"rb");
    if (!tmd) return -1;
    TMD_CONTEXT tmd_context = process_tmd(tmd);

    if(tik_context.result != 0 || tmd_context.result != 0){
        printf("[!] Input files could not be processed successfully\n");
        free(tmd_context.content_struct);
        fclose(tik);
        fclose(tmd);
        return -1;
    }

    chdir(cwd);

    int result;
    if (selected_mode == install_direct)
    {
        result = install_cia(tmd_context, tik_context);
    }
    else
    {
        FILE *output = fopen((dir + "/" + titleName + ".cia").c_str(),"wb");
        if (!output) return -2;

        result = generate_cia(tmd_context, tik_context, output);
        if(result != 0){
            remove((dir + "/" + titleName + ".cia").c_str());
        }
    }

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
    ofs.write(tikTemp, 0xA50);
    ofs.close();

    ofs.open(outputFullPath, std::ofstream::out | std::ofstream::in | std::ofstream::binary);

    //write version
    ofs.seekp(top+0xA6, std::ios::beg);
    ofs.write(titleVersion, 0x2);

    //write title id
    ofs.seekp(top+0x9C, std::ios::beg);
    ofs.write(parse_string(titleId), 0x8);

    //write key
    ofs.seekp(top+0x7F, std::ios::beg);
    ofs.write(parse_string(encTitleKey), 0x10);

    ofs.close();
}

void InstallTicket(std::string FullPath)
{
    Handle hTik;
    u32 writtenbyte;
    AM_InstallTicketBegin(&hTik);
    std::string curr = get_file_contents(FullPath.c_str());
    FSFILE_Write(hTik, &writtenbyte, 0, curr.c_str(), 0x100000, 0);
    AM_InstallTicketFinish(hTik);
    printf("Ticket Installed.");
    //delete temp ticket, ticket folder still exists... ugly. later stream directly to the handle
    remove(FullPath.c_str());
}

Result DownloadTitle(std::string titleId, std::string encTitleKey, std::string titleName)
{
    // Wait for wifi to be available
    u32 wifi = 0;
    Result ret;
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

    std::string mode_text;
    if(selected_mode == make_cia){
        mode_text = "create";
    }
    else if(selected_mode == install_direct){
        mode_text = "install";
    }


    printf("Starting - %s\n", titleName.c_str());

    mkpath((outputDir + "/tmp/").c_str(), 0777);

    // Make sure the CIA doesn't already exist
    std::string cp = outputDir + "/" + titleName + ".cia";
    char *ciaPath = new char[cp.size()+1];
    ciaPath[cp.size()]=0;
    memcpy(ciaPath,cp.c_str(),cp.size());
    if ( (selected_mode == make_cia) && FileExists(ciaPath))
    {
        printf("%s/%s.cia already exists.\n", outputDir.c_str(), titleName.c_str());
        return 0;
    }

    std::ofstream ofs;

    FILE *oh = fopen((outputDir + "/tmp/tmd").c_str(), "wb");
    if (!oh) 
    {
        printf("Error opening %s/tmp/tmd\n", outputDir.c_str());
        return -1;
    }
    Result res = DownloadFile((NUS_URL + titleId + "/tmd").c_str(), oh, false);
    fclose(oh);
    if (res != 0)
    {
        printf("Could not download TMD. Internet/Title ID is OK?\n");
        return res;
    }

    //read version
    std::ifstream tmdfs;
    tmdfs.open(outputDir + "/tmp/tmd", std::ofstream::out | std::ofstream::in | std::ofstream::binary);
    char titleVersion[2];
    tmdfs.seekg(top+0x9C, std::ios::beg);
    tmdfs.read(titleVersion, 0x2);
    tmdfs.close();

    CreateTicket(titleId, encTitleKey, titleVersion, outputDir + "/tmp/cetk");

    printf("Now %s the CIA...\n", mode_text.c_str());

    res = ConvertToCIA(outputDir + "/tmp", titleName);
    if (res != 0)
    {
        printf("Could not %s the CIA.\n", mode_text.c_str());
        return res;
    }

    if (selected_mode == make_cia)
    {
        rename((outputDir + "/tmp/" + titleName + ".cia").c_str(), (outputDir + "/" + titleName + ".cia").c_str());
    }

    printf(" DONE!\n");

    // TODO remove tmp dir

    return res;
}

void ProcessGameQueue()
{
    // Create the tickets folder if we're in ticket mode
    char empty_titleVersion[2] = {0x00, 0x00};
    if (selected_mode == install_ticket)
    {
        mkpath("/CIAngel/tickets/", 0777); 
    }

    std::vector<game_item>::iterator game = game_queue.begin();
    while(aptMainLoop() && game != game_queue.end())
    {
        std::string selected_titleid = (*game).titleid;
        std::string selected_enckey = (*game).titlekey;
        std::string selected_name = (*game).name;

        if (selected_mode == install_ticket)
        {
            CreateTicket(selected_titleid, selected_enckey, empty_titleVersion, "/CIAngel/tickets/" + selected_name + ".tik");
            InstallTicket("/CIAngel/tickets/" + selected_name + ".tik");
        }
        else
        {
            DownloadTitle(selected_titleid, selected_enckey, selected_name);
        }

        game = game_queue.erase(game);
    }

    wait_key_specific("Press A to continue.\n", KEY_A);
}

std::string getInput(HB_Keyboard* sHBKB, bool &bCancelled)
{
    sHBKB->HBKB_Clean();
    touchPosition touch;
    u8 KBState = 4;
    std::string input;
    while (KBState != 1 || input.length() == 0)
    {
        hidScanInput();
        hidTouchRead(&touch);
        KBState = sHBKB->HBKB_CallKeyboard(touch);
        input = sHBKB->HBKB_CheckKeyboardInput();

        // If the user cancelled the input
        if (KBState == 3)
        {
            bCancelled = true;
            break;
        }
        // Otherwise if the user has entered a key
        else if (KBState != 4)
        {
            printf("%c[2K\r", 27);
            printf("%s", input.c_str());
        }

        // Flush and swap framebuffers
        gfxFlushBuffers();
        gfxSwapBuffers();

        //Wait for VBlank
        gspWaitForVBlank();
    }
    printf("\n");
    return input;
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

u64 hex_to_u64( std::string value) {
    u64 out;
    std::istringstream(value) >> std::hex >> out;
    return out;
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

int levenshtein_distance(const std::string &s1, const std::string &s2)
{
    // To change the type this function manipulates and returns, change
    // the return type and the types of the two variables below.
    int s1len = s1.size();
    int s2len = s2.size();
    
    auto column_start = (decltype(s1len))1;
    
    auto column = new decltype(s1len)[s1len + 1];
    std::iota(column + column_start, column + s1len + 1, column_start);
    
    for (auto x = column_start; x <= s2len; x++) {
        column[0] = x;
        auto last_diagonal = x - column_start;
        for (auto y = column_start; y <= s1len; y++) {
            auto old_diagonal = column[y];
            auto possibilities = {
                column[y] + 1,
                column[y - 1] + 1,
                last_diagonal + (s1[y - 1] == s2[x - 1]? 0 : 1)
            };
            column[y] = std::min(possibilities);
            last_diagonal = old_diagonal;
        }
    }
    auto result = column[s1len];
    delete[] column;
    return result;
}

// Search menu keypress callback
bool menu_search_keypress(int selected, u32 key, void* data)
{
    std::vector<game_item>* cb_data = (std::vector<game_item>*)data;

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

        printf("OK - %s\n", selected_name.c_str());
        //removes any problem chars, not sure if whitespace is a problem too...?
        removeForbiddenChar(&selected_name);

        if(selected_mode == install_ticket){
            char empty_titleVersion[2] = {0x00, 0x00};
            mkpath("/CIAngel/tickets/", 0777); 
            CreateTicket(selected_titleid, selected_enckey, empty_titleVersion, "/CIAngel/tickets/" + selected_name + ".tik");
            InstallTicket("/CIAngel/tickets/" + selected_name + ".tik");
        }
        else{
            DownloadTitle(selected_titleid, selected_enckey, selected_name);
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
        wait_key_specific("\nPress A to continue.\n", KEY_A);

        return true;
    }

    return false;
}

/* Menu Action Functions */
void action_search()
{
    HB_Keyboard sHBKB;
    bool bKBCancelled = false;

    consoleClear();

    printf("Please enter text to search for:\n");
    std::string searchstring = getInput(&sHBKB, bKBCancelled);
    if (bKBCancelled)
    {
        return;
    }

    // User has entered their input, so let's scrap the keyboard
    clear_screen(GFX_BOTTOM);

    std::vector<game_item> display_output;
    for (unsigned int i = 0; i < sourceData.size(); i++){
        std::string temp;
        temp = sourceData[i]["name"].asString();
        if(temp.size() >0 && temp.find("-System") == std::string::npos && (regionFilter == "off" || sourceData[i]["region"].asString() == regionFilter)) {
            int ld = levenshtein_distance(upper(temp), upper(searchstring));
            if (ld < 10)
            {
                game_item item;
                item.ld = ld;
                item.index = i;
                item.titleid = sourceData[i]["titleid"].asString();
                item.titlekey = sourceData[i]["enckey"].asString();
                item.name = sourceData[i]["name"].asString();
                item.region = sourceData[i]["region"].asString();
                item.code = sourceData[i]["code"].asString();
                item.installed = false;

                u64 titleId = hex_to_u64(sourceData[i]["titleid"].asString());  
                FS_MediaType mediaType = ((titleId >> 32) & 0x8010) != 0 ? MEDIATYPE_NAND : MEDIATYPE_SD;
                Result res = 0;
                if( bSvcHaxAvailable && R_SUCCEEDED(res = AM_GetTitleProductCode(mediaType, titleId, nullptr)) ) {
                    item.installed = true;
                }
                display_output.push_back(item);
            }
        }
    }


    // sort similar names by levenshtein distance
    std::sort(display_output.begin(), display_output.end(), compareByLD);

    // We technically have 30 rows to work with, minus 2 for header/footer. But stick with 20 entries for now
    unsigned int display_amount = display_output.size();
    display_output.resize(display_amount);

    if (display_amount == 0)
    {
        printf("No matching titles found.\n");
        wait_key_specific("\nPress A to return.\n", KEY_A);
        return;
    }

    std::string mode_text;
    if(selected_mode == make_cia) {
        mode_text = "Create CIA";
    } else if (selected_mode == install_direct) {
        mode_text = "Install CIA";
    } else if (selected_mode == install_ticket) {
        mode_text = "Create Ticket";
    }

    char footer[51];
    char header[51];
    sprintf(header, "Select a Title (found %i results)", display_amount);
    sprintf(footer, "Press A to %s. Press X to queue.", mode_text.c_str());
    titles_multkey_draw(header, footer, 1, &display_output, &display_output, menu_search_keypress);
}

void action_prompt_queue()
{
    consoleClear();

    std::string mode_text;
    if(selected_mode == make_cia) {
        mode_text = "download";
    }
    else if (selected_mode == install_direct) {
        mode_text = "install";
    }
    else if (selected_mode == install_ticket) {
        mode_text = "create tickets for";
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
    HB_Keyboard sHBKB;
    bool bKBCancelled = false;

    consoleClear();

    // Keep looping so the user can retry if they enter a bad id/key
    while(true)
    {
        printf("Please enter a titleID:\n");
        std::string titleId = getInput(&sHBKB, bKBCancelled);
        if (bKBCancelled)
        {
            break;
        }

        printf("Please enter the corresponding encTitleKey:\n");
        std::string key = getInput(&sHBKB, bKBCancelled);
        if (bKBCancelled)
        {
            break;
        }

        if (titleId.length() == 16 && key.length() == 32)
        {
            DownloadTitle(titleId, key, "");
            wait_key_specific("\nPress A to continue.\n", KEY_A);
            break;
        }
        else
        {
            printf("encTitleKeys are 32 characters long,\nand titleIDs are 16 characters long.\n");
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
    DownloadTitle(titleId, key, "");

    wait_key_specific("\nPress A to continue.\n", KEY_A);
}

void action_toggle_install()
{
    consoleClear();

    if(selected_mode == make_cia) {
        selected_mode = install_direct;
    } else if (selected_mode == install_direct) {
        selected_mode = install_ticket;
    } else if (selected_mode == install_ticket) {
        selected_mode = make_cia;
    }
    
    if ( (selected_mode == install_ticket) || (selected_mode == install_direct) )
    {
        if (!bSvcHaxAvailable)
        {
            selected_mode = make_cia;
            printf(CONSOLE_RED "Kernel access not available.\nCan't enable Install modes.\nYou can only make a CIA.\n" CONSOLE_RESET);
            wait_key_specific("\nPress A to continue.", KEY_A);
        }
    }
}

void action_toggle_region()
{
    consoleClear();
    if(regionFilter == "off") {
        regionFilter = "ALL";
    } else if (regionFilter == "ALL") {
        regionFilter = "EUR";
    } else if (regionFilter == "EUR") {
        regionFilter = "USA";
    } else if (regionFilter == "USA") {
        regionFilter = "JPN";
    } else if (regionFilter == "JPN") {
        regionFilter = "---";
    } else if (regionFilter == "---") {
        regionFilter = "off";
    }
}

void action_about()
{
    consoleClear();

    printf(CONSOLE_RED "CIAngel by cearp and Drakia\n" CONSOLE_RESET);
    printf("Download, create, and install CIAs directly\n");
    printf("from Nintendo's CDN servers. Grabbing the\n");
    printf("latest games has never been so easy.\n");
    wait_key_specific("\nPress A to continue.\n", KEY_A);
}

void action_exit()
{
    bExit = true;
}

// Main menu keypress callback
bool menu_main_keypress(int selected, u32 key, void*)
{
    // A button triggers standard actions
    if (key & KEY_A)
    {
        switch (selected)
        {
            case 0:
                action_search();
            break;
            case 1:
                action_prompt_queue();
            break;
            case 2:
                action_manual_entry();
            break;
            case 3:
                action_input_txt();
            break;
            case 4:
                action_about();
            break;
            case 5:
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
        "Process download queue",
        "Enter a title key/ID pair",
        "Fetch title key/ID from input.txt",
        "About CIAngel",
        "Exit"
    };
    char footer[50];

    while (!bExit)
    {
        std::string mode_text;
        if(selected_mode == make_cia) {
            mode_text = "Create CIA";
        }
        else if (selected_mode == install_direct) {
            mode_text = "Install CIA";
        }
        else if (selected_mode == install_ticket) {
            mode_text = "Create Ticket";
        }

        // We have to update the footer every draw, incase the user switches install mode or region
        sprintf(footer, "Mode (L):%s Region (R):%s Queue: %d", mode_text.c_str(), regionFilter.c_str(), game_queue.size());

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

    if (bSvcHaxAvailable)
    {
        amInit();
        AM_InitializeExternalTitleDatabase(false);
    }

    init_menu(GFX_TOP);
    // Set up the reading of json
    check_JSON();
    printf("loading wings.json...\n");
    std::ifstream ifs("/CIAngel/wings.json");
    Json::Reader reader;
    Json::Value obj;
    reader.parse(ifs, obj);
    sourceData = obj; // array of characters

    menu_main();

    if (bSvcHaxAvailable)
    {
        amExit();
    }

    acExit();
    gfxExit();
    hidExit();
    httpcExit();
    socExit();
    sslcExit();
}
