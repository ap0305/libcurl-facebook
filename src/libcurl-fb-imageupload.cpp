
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <string>
#include "frozen.h"

#include <curl/curl.h>

using namespace std;

#define FB_IMAGE_UPLOAD_TEST_URL            "https://westus.api.cognitive.microsoft.com/face/v1.0/detect?returnFaceId=true&returnFaceLandmarks=false&returnFaceAttributes=age,gender"
#define FB_IMAGE_UPLOAD_APIKEY              "420ae629d4a645a5841d45526219ace1"

// get file size
ssize_t get_file_size(string filePath)
{
    struct stat st;

    // get file stat
    if (stat(filePath.c_str(), &st) != 0)
    {
        fprintf(stderr, "Could not get stat of file %s\n", filePath.c_str());
        return 0;
    }

    if (!S_ISREG(st.st_mode))
    {
        fprintf(stderr, "Invaild regular file %s\n", filePath.c_str());
        return -1;
    }

    return st.st_size;
}

struct MemoryStruct {
    char *memory;
    size_t size;
};

static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;

    mem->memory = (char*)realloc(mem->memory, realsize + 1);
    if (mem->memory == NULL) {
        /* out of memory! */
        printf("not enough memory (realloc returned NULL)\n");
        return 0;
    }

    memcpy(mem->memory, contents, realsize);
    mem->size = realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}

// debugging Web REST API process
static int DebugWAPIProc(CURL *phCurl, curl_infotype iType, char *pData, size_t iSize, void *pUser)
{
    if (iType != CURLINFO_TEXT && iType != CURLINFO_DATA_IN && iType != CURLINFO_SSL_DATA_IN)
        return 0;

    // remove end line
    if (pData[strlen(pData) - 1] == '\n')
        pData[strlen(pData) - 1] = '\0';

    if (pData[strlen(pData) - 1] == '\r')
        pData[strlen(pData) - 1] = '\0';

    fprintf(stderr, "WAPI: WAPI verbose => %s\n", pData);

    return 0;
}

// call curl API
int face_api_call_gender_age(string URL, string APIKey, string ImagePath, float& Age, int& Gender)
{
    int faceFound = 0;
    CURL *curl;
    CURLcode res;
    struct curl_slist *headers = NULL; /* http headers to send with request */

    struct MemoryStruct chunk;

    chunk.memory = (char*)malloc(1); /* will be grown as needed by the realloc above */
    chunk.size = 0; /* no data at this point */        

    FILE *fp;
    unsigned char *pData;

    ssize_t fSize;

    // get file size
    fSize = get_file_size(ImagePath.c_str());
    if (fSize == 0)
    {
        fprintf(stderr, "Invalid file size of Image file %s\n", ImagePath.c_str());
        return -1;
    }

    // read image binary data
    fp = fopen(ImagePath.c_str(), "r");
    if (!fp)
    {
        fprintf(stderr, "Could not open image file %s\n", ImagePath.c_str());
        return -1;
    }

    pData = (unsigned char *) malloc(fSize);
    fread(pData, 1, fSize, fp);

    fclose(fp);
 
    /* In windows, this will init the winsock stuff */
    curl_global_init(CURL_GLOBAL_ALL);

    /* get a curl handle */
    curl = curl_easy_init();
    if (curl) {
        /* First set the URL that is about to receive our POST. This URL can
        just as well be a https:// URL if that is what should receive the
        data. */
        curl_easy_setopt(curl, CURLOPT_URL, URL.c_str());

        headers = curl_slist_append(headers, "Content-Type: application/octet-stream");
        
        char contentLength[500];
        snprintf(contentLength, sizeof(contentLength), "Content-length: %lu", fSize);
        headers = curl_slist_append(headers, contentLength);

        char apiKeyString[500];
        snprintf(apiKeyString, sizeof(apiKeyString), "Ocp-Apim-Subscription-Key: %s", APIKey.c_str());
        headers = curl_slist_append(headers, apiKeyString);

        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, (void *) pData);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, fSize);

        /* send all data to this function */
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);

        /* we pass our 'chunk' struct to the callback function */
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);

        // set verbose
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);
        curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, DebugWAPIProc);        

        /* Perform the request, res will get the return code */
        res = curl_easy_perform(curl);
        /* Check for errors */
        if (res != CURLE_OK)
        {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        }
        else
        {
            char* attString = 0;
            int foundAttributes = json_scanf(chunk.memory + 1, chunk.size - 2, "{faceAttributes: %Q}", &attString);
            if (foundAttributes)
            {
                char* genderString = 0, *ageString = 0;

                int foundGender = json_scanf(attString, strlen(attString), "{gender: %Q}", &genderString);
                int foundAge = json_scanf(attString, strlen(attString), "{age: %Q}", &ageString);
                if (foundGender&&foundAge)
                {
                    Age = atof(ageString);
                    int genderStrCmp = strcmp(genderString, "male");
                    if (genderStrCmp == 0)
                        Gender = 0;
                    else
                        Gender = 1;

                    faceFound = 1;
                }

                free(genderString);
                free(ageString);
            }

            free(attString);
        }

        /* always cleanup */
        curl_easy_cleanup(curl);
    }

    // free buffer
    free(pData);

    // free chunk data
    free(chunk.memory);
 
    curl_global_cleanup();

    return faceFound;
}

// main function
int main(int argc, char *argv[])
{
    float age;
    int gender;

    // check argument
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s [ Image URL path ]\n", argv[0]);
        return -1;
    }

    // upload image via libcurl library
    face_api_call_gender_age(FB_IMAGE_UPLOAD_TEST_URL, FB_IMAGE_UPLOAD_APIKEY, argv[1], age, gender);

    return 0;
}
