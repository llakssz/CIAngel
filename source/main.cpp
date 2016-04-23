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

#include "utils.h"
#include "cia.h"
#include "data.h"

#include "svchax/svchax.h"
#include "json/json.h"

static const u16 top = 0x140;
static bool bSvcHaxAvailable = true;
static bool bInstallMode = false;

std::string upper(std::string s)
{
  std::string ups;
  
  for(int i = 0; i < s.size(); i++)
  {
    ups.push_back(std::toupper(s[i]));
  }
  
  return ups;
}


struct display_item {
  int ld;
  int index;
};

bool compareByLD(const display_item &a, const display_item &b)
{
    return a.ld < b.ld;
}


bool FileExists (std::string name){
    struct stat buffer;
    return (stat (name.c_str(), &buffer) == 0);
}

Result ConvertToCIA(std::string dir, std::string titleId)
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
    if (bInstallMode)
    {
        result = install_cia(tmd_context, tik_context);
    }
    else
    {
        FILE *output = fopen((dir + "/" + titleId + ".cia").c_str(),"wb");
        if (!output) return -2;

        result = generate_cia(tmd_context, tik_context, output);
        if(result != 0){
            remove((dir + "/" + titleId + ".cia").c_str());
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

Result DownloadTitle(std::string titleId, std::string encTitleKey, std::string outputDir)
{
    printf("Starting %s - %s\n", (bInstallMode ? "install" : "download"), titleId.c_str());

    mkpath((outputDir + "/tmp/").c_str(), 0777);

    // Make sure the CIA doesn't already exist
    if (!bInstallMode && FileExists(outputDir + "/" + titleId + ".cia"))
    {
        return 0;
    }

    std::ofstream ofs;

    FILE *oh = fopen((outputDir + "/tmp/tmd").c_str(), "wb");
    if (!oh) return -1;
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

    printf("Now %s the CIA...\n", (bInstallMode ? "installing" : "creating"));

    res = ConvertToCIA(outputDir + "/tmp", titleId);
    if (res != 0)
    {
        printf("Could not %s the CIA.\n", (bInstallMode ? "install" : "create"));
        return res;
    }

    if (!bInstallMode)
    {
        rename((outputDir + "/tmp/" + titleId + ".cia").c_str(), (outputDir + "/" + titleId + ".cia").c_str());
    }

    printf(" DONE!\n");
    printf("Enjoy the game :)\n");

    // TODO remove tmp dir

    return res;
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
    }
    printf("\n");
    return input;
}

void PrintMenu(bool bClear)
{
    if (bClear)
    {
        consoleClear();
    }
    
    printf("\n");
    printf("A - Read data from SD and download CIA\n");
    printf("X - Type a Key/ID pair and download CIA\n");
    printf("Y - Dl encTitleKeys.bin from 3ds.nfshost.com\n");
    printf("B - Generate tickets from encTitleKeys.bin\n");
    printf("L - Type name and download/install CIA\n");

    // Only print install mode if svchax is available
    if (bSvcHaxAvailable)
    {
        printf("R - Switch to 'Install' mode (EXPERIMENTAL!)\n");
    }

    printf("START - Exit\n");
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

int main(int argc, const char* argv[])
{
    gfxInitDefault();
    consoleInit(GFX_TOP, NULL);

    /* Sadly svchax crashes too much, so only allow install mode when running as a CIA
    // Trigger svchax so we can install CIAs
    if(argc > 0) {
        svchax_init(true);
        if(!__ctr_svchax || !__ctr_svchax_srv) {
            bSvcHaxAvailable = false;
            printf("Failed to acquire kernel access. Install mode disabled.\n");
        }
    }
    */
    
    // argc is 0 when running as a CIA, and 1 when running as a 3dsx
    if (argc > 0)
    {
        bSvcHaxAvailable = false;
    }

    u32 *soc_sharedmem, soc_sharedmem_size = 0x100000;
    httpcInit(0);
    soc_sharedmem = (u32 *)memalign(0x1000, soc_sharedmem_size);
    socInit(soc_sharedmem, soc_sharedmem_size);
    sslcInit(0);
    hidInit();

    amInit();
    AM_InitializeExternalTitleDatabase(false);
    printf("CIAngel by cearp and Drakia\n");
    PrintMenu(false);
    printf("\n");

    HB_Keyboard sHBKB;
    touchPosition touch;
    bool bKBCancelled = false;

    while (aptMainLoop())
    {
        hidScanInput();

        u32 keys = hidKeysDown();

        if (keys & KEY_A)
        {
            std::ifstream input;
            std::string titleId;
            std::string key;
            input.open("/CIAngel/input.txt", std::ofstream::in);
            GetLine(input, titleId);
            GetLine(input, key);
            DownloadTitle(titleId, key, "/CIAngel");
            
            PrintMenu(false);
        }

        if (keys & KEY_X)
        {
            printf("Please enter a titleID:\n");
            std::string titleId = getInput(&sHBKB, bKBCancelled);
            if (bKBCancelled)
            {
                bKBCancelled = false;
                PrintMenu(true);
                continue;
            }

            printf("Please enter the corresponding encTitleKey:\n");
            std::string key = getInput(&sHBKB, bKBCancelled);
            if (bKBCancelled)
            {
                bKBCancelled = false;
                PrintMenu(true);
                continue;
            }

            if (titleId.length() == 16 && key.length() == 32)
            {
                DownloadTitle(titleId, key, "/CIAngel");
                PrintMenu(false);
            }
            else
            {
                printf("encTitleKeys are 32 characters long,\nand titleIDs are 16 characters long.\nPress X to try again, or Start to exit.\n");
            }
        }

        if (keys & KEY_Y)
        {
            mkpath("/CIAngel/", 0777);

            if (FileExists("/CIAngel/encTitleKeys.bin")){
                printf("File exists... we will overwrite it!\n");
            }

            printf("Downloading encTitleKeys.bin...\n");
            FILE *oh = fopen("/CIAngel/encTitleKeys.bin", "wb");
            Result res = DownloadFile("http://3ds.nfshost.com/downloadenc", oh, true);
            fclose(oh);
            if (res != 0)
            {
                printf("Could not download file.\n");
            }
            else
            {
                printf("Downloaded OK!\n");
                PrintMenu(false);
            }
        }

        if (keys & KEY_R)
        {
            bInstallMode = !bInstallMode;
            if (bInstallMode)
            {
                if (!bSvcHaxAvailable)
                {
                    printf("Kernel access not available. Can't enable Install Mode.\n");
                    bInstallMode = false;
                }
                else
                {
                    printf("Switched to Install Mode. This is EXPERIMENTAL!\n");
                }
            }
            else
            {
                printf("Switched to Create Mode.\n");
            }
        }


        if (keys & KEY_L)
        {
            printf("Please enter text to search for the name:\n");
            std::string searchstring = getInput(&sHBKB, bKBCancelled);
            if (bKBCancelled)
            {
                bKBCancelled = false;
                PrintMenu(true);
                continue;
            }

            std::vector<display_item> display_output;
            std::ifstream ifs("/CIAngel/wings.json");
            Json::Reader reader;
            Json::Value obj;
            reader.parse(ifs, obj);
            const Json::Value& characters = obj; // array of characters
            for (unsigned int i = 0; i < characters.size(); i++){
                std::string temp;
                temp = characters[i]["name"].asString();

                int ld = levenshtein_distance(upper(temp), upper(searchstring));
                if (ld < 10)
                {
                    display_item item;
                    item.ld = ld;
                    item.index = i;
                    display_output.push_back(item);
                }

            }

            // sort similar names by levenshtein distance
            std::sort(display_output.begin(), display_output.end(), compareByLD);

            // print a max of 6 most 'similar' names. if X items, vector size is X (not X-1)
            unsigned int display_amount = 6; 
            if ( display_output.size() < display_amount )
            {
                display_amount = display_output.size();
            }

            if (display_amount == 0)
            {
                printf("No matching titles found.\n");
                PrintMenu(false);
                continue;
            }

            for(unsigned int i=0; i < display_amount; i++){
                printf( "%d - %s\n", i+1, characters[display_output[i].index]["name"].asString().c_str() );
                printf( "    %s - ", characters[display_output[i].index]["region"].asString().c_str() );
                printf( "%s\n", characters[display_output[i].index]["code"].asString().c_str() );
            }


            printf("Please enter number of game to download:\n");
            int selectednum;
            while (true)
            {
                std::string selectednumstring = getInput(&sHBKB, bKBCancelled);
                if (bKBCancelled)
                {
                    break;
                }
                
                std::stringstream iss(selectednumstring);
                iss >> std::ws >> selectednum >> std::ws;

                // Make sure the number was valid
                if (!iss.eof())
                {
                    printf("Invalid. Please enter number of game to download:\n");
                    continue;
                }

                break;
            }

            if (bKBCancelled)
            {
                PrintMenu(true);
                bKBCancelled = false;
                continue;
            }

            selectednum--;

            std::string selected_titleid = characters[display_output[selectednum].index]["titleid"].asString();
            std::string selected_enckey = characters[display_output[selectednum].index]["enckey"].asString();
            std::string selected_name = characters[display_output[selectednum].index]["name"].asString();
            
            printf("OK - %s\n", selected_name.c_str());

            DownloadTitle(selected_titleid, selected_enckey, "/CIAngel/" + selected_name);
            PrintMenu(false);
        }

        if (keys & KEY_B)
        {

            mkpath("/CIAngel/tickets/", 0777);

            // we don't really need to get the version number from the TMD and inject it to the ticket, it is nice but not really needed, especially when just making a ticket
            // makecdncia warns about it but build a good cia, it doesn't stop a cia being legit (if we are building a legit cia) if the tmd version doesn't match the ticket version.
            char titleVersion[2] = {0x00, 0x00};
            int count = 0;
            std::ifstream keyfile("/CIAngel/encTitleKeys.bin", std::ifstream::binary);
            keyfile.seekg(0x10, std::ios::beg);
            std::vector<char> buffer (0x20,0);

            while(keyfile.read(buffer.data(), buffer.size()))
            {
                std::string titleId = "";
                std::string encTitleKey = "";

                for (u16 i=0x8; i<0x10; i++)
                {
                    titleId = titleId + buffer[i];
                }
                for (u16 i=0x10; i<0x20; i++)
                {
                    encTitleKey = encTitleKey + buffer[i];
                }

                titleId = ToHex(titleId);
                encTitleKey = ToHex(encTitleKey);

                printf("title id - %s\n", titleId.c_str());
                printf("key - %s\n", encTitleKey.c_str());

                count++;

                CreateTicket(titleId, encTitleKey, titleVersion, "/CIAngel/tickets/" + titleId + ".tik");
                

                // **** my python code to port ****
                // titleid = binascii.hexlify(block[0x8:0x10])
                // key = binascii.hexlify(block[0x10:0x20])
                // typecheck = titleid[4:8]
                
                // if arguments.all:
                //     #skip updates
                //     if (typecheck == '000e'):
                //         continue
                //     #skip system
                //     if (int(typecheck,16) & 0x10):
                //         continue
                //     elif (typecheck == '8005'):
                //         continue
                //     elif (typecheck == '800f'):
                //         continue
                // if arguments.all or (titleid in titlelist):
                //     processContent(titleid, key)



            }
            printf("%d tickets dumped to sd:/CIAngel/tickets/\n", count);
            PrintMenu(false);

        }

        

        if (keys & KEY_START) break;

        //Prevent keyboard flicker after failed input attempt
        hidTouchRead(&touch);
        sHBKB.HBKB_CallKeyboard(touch);

        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }

    amExit();

    gfxExit();
    hidExit();
    httpcExit();
    socExit();
    sslcExit();
}
