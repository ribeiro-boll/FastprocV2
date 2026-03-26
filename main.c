#include <locale.h>
#include <sched.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <ctype.h>
#include <threads.h>
#include <time.h>
#include <unistd.h>
#include <ncurses.h>
#include <pthread.h>
#include <limits.h>
#include <signal.h>

pthread_mutex_t mutex;
pthread_cond_t thcond;

char* sortMethods[] = {"RAM",  "CPU", "Threads","PID"};
int currSortMethod = 0;
char charSequence[32] = "";
int charSequenceCurrIndex = 0;
double cpuTotalUsage = 0.0;
double cpuTotalUsageCopy = 0.0;
float temperatureAverage = 0.0;
float temperatureAverageCopy = 0.0;
float *temperatureCores = NULL;
float *temperatureCoresCopy = NULL;
int contagemCores = 0;
int contagemCoresCopy = 0;


struct Process {
    char *p_name;
    char *p_state;
    int p_pid;
    long int p_ram;
    int p_threads;

    long long int p_cpuNewCount;

    double p_cpuPercent;

} typedef processStruct;

typedef struct Node {
    int pid;
    long long int cpu_time;
    struct Node *next;
} Node;

typedef struct {
    Node **buckets;
    int size;
} HashMap;

void swap_processes(processStruct *a, processStruct *b) {
    processStruct tmp = *a;
    *a = *b;
    *b = tmp;
}

void free_process_list(processStruct *list, int count) {
    if (!list) return;

    for (int i = 0; i < count; i++) {
        free(list[i].p_name);
        free(list[i].p_state);
    }

    free(list);
}

processStruct *deep_copy_process_list(processStruct *src, int count) {
    if (!src || count <= 0) return NULL;

    processStruct *copy = malloc(sizeof(processStruct) * count);

    for (int i = 0; i < count; i++) {
        copy[i].p_pid = src[i].p_pid;
        copy[i].p_ram = src[i].p_ram;
        copy[i].p_threads = src[i].p_threads;
        copy[i].p_cpuNewCount = src[i].p_cpuNewCount;
        copy[i].p_cpuPercent = src[i].p_cpuPercent;
        copy[i].p_name = src[i].p_name ? strdup(src[i].p_name) : NULL;
        copy[i].p_state = src[i].p_state ? strdup(src[i].p_state) : NULL;
    }

    return copy;
}

int hash(int pid, int size) {
    return pid % size;
}

HashMap* create_map(int size) {
    HashMap *map = malloc(sizeof(HashMap));
    map->size = size;
    map->buckets = calloc(size, sizeof(Node*));
    return map;
}

HashMap *hs;

Node* get(HashMap *map, int pid) {
    int idx = hash(pid, map->size);
    Node *cur = map->buckets[idx];

    while (cur) {
        if (cur->pid == pid)
            return cur;
        cur = cur->next;
    }
    return NULL;
}

void put(HashMap *map, int pid, long cpu_time) {
    int idx = hash(pid, map->size);
    Node *cur = map->buckets[idx];

    while (cur) {
        if (cur->pid == pid) {
            cur->cpu_time = cpu_time;
            return;
        }
        cur = cur->next;
    }
    Node *new = malloc(sizeof(Node));
    new->pid = pid;
    new->cpu_time = cpu_time;
    new->next = map->buckets[idx];
    map->buckets[idx] = new;
}

void free_map(HashMap *map) {
    for (int i = 0; i < map->size; i++) {
        Node *cur = map->buckets[i];
        while (cur) {
            Node *tmp = cur;
            cur = cur->next;
            free(tmp);
        }
    }
    free(map->buckets);
    free(map);
}
int getNmbrOfCores(){
    int nmbrOfCores = 0;
    char* buffer = malloc(2048);
    FILE *cpuinfo = fopen("/proc/cpuinfo", "r");
    if (cpuinfo == 0){
        return 0;
    }
    while (fgets(buffer, 2048, cpuinfo) != NULL) {
        buffer[9] = '\0';
        if (strcmp(buffer, "processor")==0){
            nmbrOfCores++;
        }
    }
    free(buffer);
    fclose(cpuinfo);
    return nmbrOfCores;
}

long long int convert_int(char* arr){
    long long int retorno_nmr =0;
    for (int i = 0;i<(int)strlen(arr);i++){
        switch (arr[i]) {
        case '0': retorno_nmr*=10;break;
        case '1':case '2':case '3':
        case '4':case '5':case '6':
        case '7':case '8':case '9':
            retorno_nmr = retorno_nmr*10 + (arr[i] - '0');break;
        default:break;
        }
    }
    return retorno_nmr;
}

void addToTempList(char* temporaryPath){
    FILE* f = fopen(temporaryPath, "r");
    if (f == NULL) return;
    char buffer[32];
    fgets(buffer, 32, f);
    float temp = convert_int(buffer)/1000;
    if (temperatureCores == NULL){
        temperatureCores = malloc(sizeof(float));
        contagemCores++;
        temperatureCores[contagemCores-1] = temp;
        temperatureAverage = temp;
    }
    else{
        contagemCores++;
        temperatureCores = realloc(temperatureCores,sizeof(float)*contagemCores);
        temperatureCores[contagemCores-1] = temp;
        temperatureAverage += temp;
    }
    fclose(f);
}

void getCpuTemp(char* path){
    DIR* diretorio = opendir(path);
    if (diretorio == NULL){
        return;
    }
    struct dirent *entity = readdir(diretorio);
    while (entity!=NULL) {
        if (strcmp(entity->d_name,".") == 0 || strcmp(entity->d_name,"..") == 0) {
            entity = readdir(diretorio);
            continue;
        }
        else if (strncmp(entity->d_name, "thermal_zone", 12)==0) {
            char temporaryPath[1024];
            snprintf(temporaryPath, 1024, "%s/%s/%s", path,entity->d_name,"temp");
            addToTempList(temporaryPath);
            entity = readdir(diretorio);
            continue;
        }
        entity = readdir(diretorio);
    }
    closedir(diretorio);
}


long long int getSystemCpuTime(){
    char* buffer = malloc(2048);
    FILE *cpuinfo = fopen("/proc/stat", "r");
    if (cpuinfo == NULL){
        return 0;
    }
    fgets(buffer, 2048, cpuinfo);
    char* token = strtok(buffer, " ");
    token = strtok(NULL, " ");
    long long int cpuPercent = 0;
    for (int i =0;i<8; i++) {
        cpuPercent+= convert_int(token);
        token = strtok(NULL, " ");
    }
    free(buffer);
    fclose(cpuinfo);
    return cpuPercent;
}

int pageSize;
int systemHertz;
int nmbrOfCores;

long long int cpuOldTickCount;
long long int cpuNewTickCount;

processStruct *processList = NULL;
processStruct *processListNcurses = NULL;
processStruct *processListFilteredTemporary = NULL;

int processNmbr = 0;
int processNmbrNcurses = 0;
int processNmbrFilteredTemporary = 0;

void sort_array_name(processStruct *list, char charSequence[]){
    for (int i = 0; i< processNmbr; i++) {
        if (charSequenceCurrIndex > 0){
            char tempBufferLower[256];
            for(int j = 0; j < strlen(list[i].p_name); j++){
                tempBufferLower[j] = tolower(list[i].p_name[j]);
            }
            tempBufferLower[strlen(list[i].p_name)] = '\0';
            if (strncmp(tempBufferLower, charSequence, charSequenceCurrIndex) == 0) {
                if (processListFilteredTemporary == NULL){
                    processListFilteredTemporary = (processStruct*)malloc(sizeof(processStruct));
                    processNmbrFilteredTemporary = 1;
                }
                else {
                    processNmbrFilteredTemporary++;
                    processListFilteredTemporary = (processStruct*)realloc(processListFilteredTemporary, processNmbrFilteredTemporary * sizeof(processStruct));
                }
                processListFilteredTemporary[processNmbrFilteredTemporary-1].p_name           = strdup(list[i].p_name);
                processListFilteredTemporary[processNmbrFilteredTemporary-1].p_state          = strdup(list[i].p_state);
                processListFilteredTemporary[processNmbrFilteredTemporary-1].p_cpuPercent     = list[i].p_cpuPercent;
                processListFilteredTemporary[processNmbrFilteredTemporary-1].p_pid            = list[i].p_pid;
                processListFilteredTemporary[processNmbrFilteredTemporary-1].p_ram            = list[i].p_ram;
                processListFilteredTemporary[processNmbrFilteredTemporary-1].p_threads        = list[i].p_threads;
                processListFilteredTemporary[processNmbrFilteredTemporary-1].p_cpuNewCount    = list[i].p_cpuNewCount;
            }
        }
        else if (charSequenceCurrIndex == 0) {
            if (processListFilteredTemporary == NULL){
                    processListFilteredTemporary = (processStruct*)malloc(sizeof(processStruct));
                    processNmbrFilteredTemporary = 1;
                }
            else {
                processNmbrFilteredTemporary++;
                processListFilteredTemporary = (processStruct*)realloc(processListFilteredTemporary, processNmbrFilteredTemporary * sizeof(processStruct));
            }
            processListFilteredTemporary[processNmbrFilteredTemporary-1].p_name           = strdup(list[i].p_name);
            processListFilteredTemporary[processNmbrFilteredTemporary-1].p_state          = strdup(list[i].p_state);
            processListFilteredTemporary[processNmbrFilteredTemporary-1].p_cpuPercent     = list[i].p_cpuPercent;
            processListFilteredTemporary[processNmbrFilteredTemporary-1].p_pid            = list[i].p_pid;
            processListFilteredTemporary[processNmbrFilteredTemporary-1].p_ram            = list[i].p_ram;
            processListFilteredTemporary[processNmbrFilteredTemporary-1].p_threads        = list[i].p_threads;
            processListFilteredTemporary[processNmbrFilteredTemporary-1].p_cpuNewCount    = list[i].p_cpuNewCount;
        }
    } 
}
void sort_array_threads(processStruct *list, int processCount){
    int currMaxIndex = processCount;
    for (int i = 0; i < processNmbr; i++) {
        int min = 99999;
        int index_min = 0;
        for (int j = 0; j < currMaxIndex; j++) {
            if (list[j].p_threads < min){
                index_min = j;
                min = list[j].p_threads;
            }
        }
        swap_processes(&list[index_min], &list[currMaxIndex-1]);
        currMaxIndex--;
    }
}

void sort_array_cpu(processStruct *list, int processCount){
    int currMaxIndex = processCount;
    for (int i = 0; i < processNmbr; i++) {
        float min = 999.0;
        int index_min = 0;
        for (int j = 0; j < currMaxIndex; j++) {
            if (list[j].p_cpuPercent < min){
                index_min = j;
                min = list[j].p_cpuPercent;
            }
        }
        swap_processes(&list[index_min], &list[currMaxIndex-1]);
        currMaxIndex--;
    }
}

void sort_array_pid(processStruct *list, int processCount){
    int currMaxIndex = processCount;
    for (int i = 0; i < processNmbr; i++) {
        int min = 999999999;
        int index_min = 0;
        for (int j = 0; j < currMaxIndex; j++) {
            if (list[j].p_pid < min){
                index_min = j;
                min = list[j].p_pid;
            }
        }
        swap_processes(&list[index_min], &list[currMaxIndex-1]);
        currMaxIndex--;
    }
}

void sort_array_mem(processStruct *list, int processCount){
    int currMaxIndex = processCount;
    for (int i = 0; i < processNmbr; i++) {
        long long int min = LLONG_MAX;
        int index_min = 0;
        for (int j = 0; j < currMaxIndex; j++) {
            if (list[j].p_ram < min){
                index_min = j;
                min = list[j].p_ram;
            }
        }
        swap_processes(&list[index_min], &list[currMaxIndex-1]);
        currMaxIndex--;
    }
}
void populateProcessStruct(char* pathToStatus, processStruct *process){
    FILE* statusFile = fopen(pathToStatus, "r");
    if (statusFile == NULL){
        process->p_cpuNewCount = 0;
        process->p_cpuPercent = 0;
        process->p_pid = 0;
        process->p_ram = 0;
        process->p_state = strdup(" ");
        process->p_name = strdup(" ");
        process->p_threads = 0;
        return;
    }
    char* buffer = malloc(2048);
    fgets(buffer, 2048, statusFile);
    char* token = strtok(buffer, " ");
    int contador = 0;
    int contadorDpsNome = 0;
    int condDpsNome = 0;
    int condEspacosNoNome = 0;
    char tempBufferNameWithSpace[4096] = "";;
    long long int totalUptime;
    char* tempBuffer;
    while (token!=NULL) {
        tempBuffer = NULL;
        if (contador == 0){
            process->p_pid = convert_int(token); 
        }

        else if (contador == 1) {
            tempBuffer = strdup(token);
        
            if (tempBuffer[0] == '(' && tempBuffer[strlen(tempBuffer)-1] == ')'){
                char *tmp = strdup(tempBuffer + 1);
                tmp[strlen(tmp) - 1] = '\0';
                process->p_name = tmp;
                condDpsNome = 1;
                free(tempBuffer);
            }
            else {
                condEspacosNoNome = 1;
                if (condEspacosNoNome){
                    contador--;
                }
                if (tempBuffer[0] == '(' && strlen(tempBuffer) != 1){
                    char *originalPtr = tempBuffer; 
                    tempBuffer++;
                    snprintf(tempBufferNameWithSpace, 4096, "%s ", tempBuffer);
                    free(originalPtr);
                }
                else if (tempBuffer[strlen(tempBuffer)-1] == ')'){
                    strncat(tempBufferNameWithSpace, tempBuffer, 4096 - strlen(tempBufferNameWithSpace) - 1);
                    process->p_name = strdup(tempBufferNameWithSpace);
                    process->p_name[strlen(process->p_name)-1] = '\0';
                    condDpsNome = 1;
                    contador++;
                    condEspacosNoNome = 0;
                    free(tempBuffer);
                }
                else {
                    strncat(tempBufferNameWithSpace, tempBuffer, 4096 - strlen(tempBufferNameWithSpace) - 1);
                    strncat(tempBufferNameWithSpace, " ", 4096 - strlen(tempBufferNameWithSpace) - 1);
                    free(tempBuffer);
                }
            }
        }
        else if (contadorDpsNome == 1) {
            process->p_state = strdup(token);
        }
        else if (contadorDpsNome == 12) {
            totalUptime = convert_int(token);
        }
        else if (contadorDpsNome == 13) {
            process->p_cpuNewCount = totalUptime + convert_int(token);
            Node * iten = get(hs, process->p_pid);
            if (iten == NULL || iten->cpu_time == 0){
                process->p_cpuPercent = 0;
                totalUptime = 0;
            }
            else {
                process->p_cpuPercent = ((double)(process->p_cpuNewCount - iten->cpu_time)/(( cpuNewTickCount-cpuOldTickCount))); 
                cpuTotalUsage+=process->p_cpuPercent;
                totalUptime = 0;
            }
        }
        else if (contadorDpsNome == 18) {
            process->p_threads = convert_int(token);
        }
        else if (contadorDpsNome == 22) {
            process->p_ram = convert_int(token) * pageSize;
        }
        if (condDpsNome) {
            contadorDpsNome++;
        }
        token = strtok(NULL, " ");
        contador++;
    }
    /*
    pid                   ->        0 espaço
    nome - * entre "()" * ->        1 espaço
    p_threads	          ->        18 lugar dps do nome  
    rss	                  ->       22 lugar dps do nome  
    */
    free(buffer);
    fclose(statusFile);
}

void getOldPidCpuTime(char* pathToStatus){
    FILE* statusFile = fopen(pathToStatus, "r");
    if (statusFile == NULL){
        return;
    }
    char* buffer = malloc(2048);
    fgets(buffer, 2048, statusFile);
    char* token = strtok(buffer, " ");
    int contador = 0;
    int contadorDpsNome = 0;
    int condDpsNome = 0;
    long long int totalUptime;
    int pid;
    while (token!=NULL) {
        if (contador == 0 ){
            pid = convert_int(token);
        }
        else if (contador == 1) {    
            if (token[strlen(token)-1] == ')'){
                condDpsNome = 1;
                contador++;
            }
            else {
                token = strtok(NULL, " ");
                continue;
            }
        }
        else if (contadorDpsNome == 12) {
            totalUptime = convert_int(token);
        }
        else if (contadorDpsNome == 13) {
            put(hs, pid, totalUptime + convert_int(token));
            totalUptime = 0;
        }
        if (condDpsNome) {
            contadorDpsNome++;
        }
        token = strtok(NULL, " ");
        contador++;
    }
    /*
    pid                   ->        0 espaço
    nome - * entre "()" * ->        1 espaço
    p_threads	          ->        18 lugar dps do nome  
    rss	                  ->       22 lugar dps do nome  
    */
    free(buffer);
    fclose(statusFile);
}


void addToProcessList(char* pathToStatus){
    if (processList == NULL){
        processList = (processStruct*)malloc(sizeof(processStruct));
        processNmbr = 1;
        populateProcessStruct(pathToStatus,&processList[processNmbr-1]);
    }
    else {
        processNmbr++;
        processList = (processStruct*)realloc(processList, processNmbr* sizeof(processStruct));
        populateProcessStruct(pathToStatus,&processList[processNmbr-1]);
    }

}

int isProcess(char* fileName){
    for (int i = 0; i < strlen(fileName); i++){
        if (!isdigit(fileName[i])){
            return 0;
        }
    }
    return 1;
}

void testRecursion(char* path){
    DIR* diretorio = opendir(path);
    if (diretorio == NULL){
        return;
    }
    struct dirent *entity = readdir(diretorio);
    while (entity!=NULL) {
        if (strcmp(entity->d_name,".") == 0 || strcmp(entity->d_name,"..") == 0) {
            entity = readdir(diretorio);
            continue;
        }
        else if (isProcess(entity->d_name)) {
            char temporaryPath[1024];
            snprintf(temporaryPath, 1024, "%s/%s", path,entity->d_name);
            testRecursion(temporaryPath);
            entity = readdir(diretorio);
            continue;
        }
        else if (strcmp(entity->d_name, "stat")==0 && strcmp(path, "/proc")!=0) {
            char temporaryPath[1024];
            snprintf(temporaryPath, 1024, "%s/%s", path,entity->d_name);
            addToProcessList(temporaryPath);
            closedir(diretorio);
            return;
        }
        entity = readdir(diretorio);
    }
    closedir(diretorio);
}

void testRecursionOldCpu(char* path){
    DIR* diretorio = opendir(path);
    if (diretorio == NULL){
        return;
    }
    struct dirent *entity = readdir(diretorio);
    while (entity!=NULL) {
        if (strcmp(entity->d_name,".") == 0 || strcmp(entity->d_name,"..") == 0) {
            entity = readdir(diretorio);
            continue;
        }

        else if (isProcess(entity->d_name)) {
            char temporaryPath[1024];
            snprintf(temporaryPath, 1024, "%s/%s", path,entity->d_name);
            testRecursionOldCpu(temporaryPath);
            entity = readdir(diretorio);
            continue;
        }

        else if (strcmp(entity->d_name, "stat")==0 && strcmp(path, "/proc")!=0) {
            char temporaryPath[1024];
            snprintf(temporaryPath, 1024, "%s/%s", path,entity->d_name);
            getOldPidCpuTime(temporaryPath);
            closedir(diretorio);
            return;
        }

        entity = readdir(diretorio);

    }
    closedir(diretorio);
}
_Atomic int breakLoop = 0;
WINDOW *create_newwin(int height, int width, int starty, int startx){	WINDOW *local_win;

	local_win = newwin(height, width, starty, startx);
	//box(local_win, 0 , 0);
	wrefresh(local_win);	

	return local_win;
}

float *cpyTempArr(float *src, int count) {
    if (!src || count <= 0) return NULL;

    float *copy = malloc(sizeof(float) * count);

    for (int i = 0; i < count; i++) {
        copy[i] = src[i];
    }

    return copy;
}

void *routine(void* arg){
    while (1) {
        int filteredByNameCond = 0;
        hs = NULL;
        processList = NULL; 
        processListFilteredTemporary = NULL;
        temperatureCores = NULL;
        //temperatureCoresCopy = NULL;
        hs = create_map(1024);
        getCpuTemp("/sys/class/thermal");
        cpuOldTickCount = getSystemCpuTime();
        testRecursionOldCpu("/proc");
        sleep(1);
        cpuNewTickCount = getSystemCpuTime();
        testRecursion("/proc");
        switch (currSortMethod) {
            case 0: sort_array_mem(processList,processNmbr);    break;
            case 1: sort_array_cpu(processList,processNmbr);    break;
            case 2: sort_array_threads(processList,processNmbr);break;
            case 3: sort_array_pid(processList,processNmbr);    break;
        }
        sort_array_name(processList,charSequence);
        filteredByNameCond = 1;

        pthread_mutex_lock(&mutex);
        free_process_list(processListNcurses, processNmbrNcurses);
        free(temperatureCoresCopy);
        temperatureCoresCopy = NULL;
        processListNcurses = NULL;
        temperatureCoresCopy = cpyTempArr(temperatureCores,contagemCores);
        temperatureAverageCopy = temperatureAverage/contagemCores;
        processListNcurses = deep_copy_process_list(processListFilteredTemporary, processNmbrFilteredTemporary);
        processNmbrNcurses = processNmbrFilteredTemporary;
        cpuTotalUsageCopy = cpuTotalUsage;
        free_process_list(processListFilteredTemporary, processNmbrFilteredTemporary);
        free_process_list(processList, processNmbr);
        free(temperatureCores);
        contagemCoresCopy = contagemCores;
        pthread_mutex_unlock(&mutex);

        contagemCores = 0;
        processNmbrFilteredTemporary = 0;
        processNmbr = 0;
        cpuTotalUsage = 0.0;
        free_map(hs);
        if (breakLoop){
            break;
        }
    }
    return NULL;
}

int main(){
    int showCredits = 1;
    setlocale(LC_ALL, ".UTF8");
    pthread_t t1;
    WINDOW *win1, *win2, *win3, *win4, *winTemp, *winInfo;
    pthread_mutex_init(&mutex, NULL);
	int startx, starty, width, height;
	int ch;
    int maxElements = 11;
    int condTyping = 0;
    char* lastClosedProcess = strdup("~~~~~~~~~~~~~");
	initscr();
	cbreak();
    keypad(stdscr, TRUE);
    curs_set(0);
    noecho();
    nodelay(stdscr, TRUE);
    height = 14;
	width = 72;
	starty = (LINES - height) / 2 - 10;
	startx = (COLS - width) / 2;
	win1 = create_newwin(height - 1, width, starty+1, startx);
    winInfo = create_newwin(height/2 - 3, width, starty + 14, startx);

    win2 =      create_newwin(height/2 - 3, width, starty + 14 + 4, startx);
    winTemp =   create_newwin(height/2 - 3, width, starty + 14 + 4 + 4 + 4, startx);
    win3 =      create_newwin(height/2 - 3, width, starty + 14 + 4 + 4, startx);
    win4 =      create_newwin(height/2  -3, width, starty + 14 + 4 + 4 + 4 + 4, startx);
    processList = NULL;
    processListNcurses = NULL;
    pageSize = sysconf(_SC_PAGESIZE);
    systemHertz = sysconf(_SC_CLK_TCK);
    nmbrOfCores = getNmbrOfCores();


    //wrefresh(my_win);
    pthread_create(&t1, NULL, routine, NULL);
    sleep(2);
    int selected = 0;
    int inicio = 0;
    char linha_texto[1024];
    double mem1 =1, mem2=1;
    double memoria_total_pc = 0.0;
    double memoria_usada_atual = 0.0;

    double percentage = 0;
    int contadorLastValidProcess = 0;
    int condKeyDown;
    int snapshotCount;
    double cpuTotalUsageCopySnapshot;
    float cpuAverageTempSnapshot;

    int contagemCoresSnapshot;
    while (1) {
        condKeyDown = 0;
        pthread_mutex_lock(&mutex);
        processStruct *listSnapshot = deep_copy_process_list(processListNcurses, processNmbrNcurses);
        float *cpuTempSnapshot = cpyTempArr(temperatureCoresCopy,contagemCoresCopy);
        snapshotCount = processNmbrNcurses;
        cpuTotalUsageCopySnapshot = cpuTotalUsageCopy;
        cpuAverageTempSnapshot = temperatureAverageCopy;
        contagemCoresSnapshot = contagemCoresCopy;
        pthread_mutex_unlock(&mutex);
        FILE *meminfo = fopen("/proc/meminfo", "r");
        if(meminfo) {
            while (fgets(linha_texto, sizeof(linha_texto), meminfo) != NULL){
                if (strncmp(linha_texto, "MemAvailable:", 13) == 0){
                    sscanf(linha_texto + 14, "%lf", &mem2);
                } 
                else if (strncmp(linha_texto, "MemTotal:", 9) == 0){
                    sscanf(linha_texto + 10, "%lf", &mem1);
                }
            }
            memoria_total_pc = mem1/(1024*1024);
            memoria_usada_atual = ((mem1-mem2)/(1024*1024));
            percentage = (memoria_usada_atual/memoria_total_pc)*100;
            fclose(meminfo);
        }
        box(win1, 0, 0);
        box(win2, 0, 0);
        box(win3, 0, 0);
        box(win4, 0, 0);
        box(winInfo, 0, 0);
        box(winTemp, 0, 0);
        mvwprintw(win1, 0, 2, "> Current Process List <");
        if (showCredits){
            mvwprintw(win4,5,37, "(Author: Lucca Ribeiro, Bolota :D)");
        }   
        for (int i = 0; i < maxElements ; i++){
            int index_ncurses = inicio + i-3;
            if (i == 0) {
                mvwprintw(win1, i+1, 4,    "________________________________________________________________  ");
                continue;
            }
            if (i == 1){
                mvwprintw(win1, i+1, 1, "  |    PID    |       name       | threads |      ram      |  CPU  | ");
                continue;
            }
            if (i == 2) {
                mvwprintw(win1, i+1, 1, "  |-----------|------------------|---------|---------------|-------| ");
                continue;
            }
            if (i == maxElements-1) {
                mvwprintw(win1, i+1, 1, "  !___________!__________________!_________!_______________!_______! ");
                continue;
            }
            else if (i<snapshotCount+3 && index_ncurses < snapshotCount && index_ncurses >=0) {
                mvwprintw(win1, i+1, 3, "|");
                if (index_ncurses == selected) {
                    wattron(win1, A_REVERSE);
                }
                if(listSnapshot[index_ncurses].p_ram  > (long long int)(1024*1024*1024)){
                    mvwprintw(win1, i+1, 4, "  %7d  | %-16.15s | %6.d  |  %6.1f %-5.4s | %4.1lf%% ", listSnapshot[index_ncurses].p_pid,listSnapshot[index_ncurses].p_name,listSnapshot[index_ncurses].p_threads,listSnapshot[index_ncurses].p_ram/(1024.0*1024*1024)," GBs", listSnapshot[index_ncurses].p_cpuPercent * 100);
                }
                if (listSnapshot[index_ncurses].p_ram < (long long int)(1024*1024*1024)){
                    mvwprintw(win1, i+1, 4, "  %7d  | %-16.15s | %6.d  |  %6.1f %-5.4s | %4.1lf%% ", listSnapshot[index_ncurses].p_pid,listSnapshot[index_ncurses].p_name,listSnapshot[index_ncurses].p_threads,listSnapshot[index_ncurses].p_ram/(1024.0*1024)," MBs", listSnapshot[index_ncurses].p_cpuPercent * 100);
                }
                contadorLastValidProcess = index_ncurses;
                if (index_ncurses == selected) {
                    wattroff(win1, A_REVERSE);
                }
                mvwprintw(win1, i+1, 68, "|");
            }
            else {
                mvwprintw(win1, i+1, 1, "  |           |                  |         |               |       | ");
            }
        }
        ch = getch();
        if (condTyping){
            switch (ch) {
                case '\n':
                    condTyping = 0; break;
                case KEY_BACKSPACE:
                    if (charSequenceCurrIndex > 0){
                        charSequence[charSequenceCurrIndex-1] = '\0';
                        charSequenceCurrIndex--;
                    }
                    break;

                case 'a': case 'b': case 'c': case 'd': case 'e':
                case 'f': case 'g': case 'h': case 'i': case 'j':
                case 'k': case 'l': case 'm': case 'n': case 'o':
                case 'p': case 'q': case 'r': case 's': case 't':
                case 'u': case 'v': case 'w': case 'x': case 'y':
                case 'z':

                case 'A': case 'B': case 'C': case 'D': case 'E':
                case 'F': case 'G': case 'H': case 'I': case 'J':
                case 'K': case 'L': case 'M': case 'N': case 'O':
                case 'P': case 'Q': case 'R': case 'S': case 'T':
                case 'U': case 'V': case 'W': case 'X': case 'Y':
                case 'Z':

                case '0': case '1': case '2': case '3': case '4':
                case '5': case '6': case '7': case '8': case '9':

                case '-': case '_': case ' ': case '\\': case '/':
                case '=': case '+': case '(': case ')': case '!':
                case '@': case '#': case '%': case '&': case '*':
                case '|': case ';': case ':': case '>': case '<':
                case '.': case '?':
                if (charSequenceCurrIndex<32) {
                    charSequence[charSequenceCurrIndex] = tolower(ch);
                    charSequenceCurrIndex++;
                    selected = 0;
                    inicio = 0;
                }
                break;
            }
        }
        else {
            switch (ch) {
                case 'w':
                case 'W':
                case KEY_UP:
                    if (selected - inicio > 0) {
                        selected--;
                        if (selected - 1 < inicio && inicio > 0) {
                            inicio--;
                        }
                    }
                    break;
                
                case 's':
                case 'S':
                case KEY_DOWN:
                    if (snapshotCount - selected> 1) {
                        selected++;
                        condKeyDown = 1;
                        if (selected  > inicio + maxElements - 5 && inicio +  maxElements - 5< snapshotCount+3) {
                            inicio++;
                        }
                    }
                    break;

                case 'k':
                case 'K': {
                    free(lastClosedProcess);
                    lastClosedProcess = NULL;
                    lastClosedProcess = strdup(listSnapshot[selected].p_name);
                    long codigo = listSnapshot[selected].p_pid;
                    kill(codigo, SIGTERM);
                    break;
                }
            
                case '1':
                    currSortMethod = 0;
                    break;
            
                case '2':
                    currSortMethod = 1;
                    break;
            
                case '\n':
                    condTyping = 1;
                    break;
            
                case '3':
                    currSortMethod = 2;
                    break;
            
                case '4':
                    currSortMethod = 3;
                    break;
            
                case 'q':
                case 'Q':
                    breakLoop = 1;
                    break;
            
                default:
                    break;
            }
        }
        // if (selected   > snapshotCount){
        //     selected = selected - (selected-(snapshotCount -1));
        // }
        if (selected > contadorLastValidProcess && !condKeyDown){
            selected = contadorLastValidProcess;
        }
        if (selected<inicio){
            selected = inicio;
        }
        mvwprintw(win2,0,2,    "> Basic Controls <");
        mvwprintw(win2,0,27,   "> Last Closed Process: %-15s <", lastClosedProcess);
        mvwprintw(win2,1,2,    " Press 'Arrow up' to move up    |  Press 'Arrow down' to move down");
        mvwprintw(win2,2,2,    " Press 'k' to close a task      |  Press 'q' to exit the program");
        
        mvwprintw(winInfo,0,2, "> Sorting Methods Controls <");
        mvwprintw(winInfo,0,38,"> Current Sort Method: %s <", sortMethods[currSortMethod]);
        mvwprintw(winInfo,1,2, "Press '1' to sort by Used RAM   | Press '3' to sort by Threads");
        mvwprintw(winInfo,2,2, "Press '2' to sort by CPU usage  | Press '4' to sort by PID");

        mvwprintw(winTemp,0,2, "> System Temperature <");
        mvwprintw(winTemp,1,2, "CPU average temperature: %3.0fC - only the first 7 cores will be shown",cpuAverageTempSnapshot);
        mvwprintw(winTemp,2,2, "Cores temperature -> ");
        int contadorChars= 24;
        for (int i = 0; i<contagemCoresSnapshot; i++){
            if (contadorChars == 42){
                break;
            }
            mvwprintw(winTemp,2,contadorChars, "[%3.0fC]",cpuTempSnapshot[i]);
            contadorChars+=6;
        }



        mvwprintw(win3,0,2,    "> System Info <");
        mvwprintw(win3,1,2,    "Current RAM in Use:  %.2f GBs   | Total RAM Avaliable: %.2f GBs", memoria_usada_atual,memoria_total_pc);
        mvwprintw(win3,2,2,    "Current RAM in Percent: %.2f%-2s | Current CPU Usage: %3.2lf%-3s",percentage,"%", cpuTotalUsageCopySnapshot*100,"%");

        mvwprintw(win4,0,2,    "> Search By Name <");
        mvwprintw(win4,1,2,    "Press 'Enter' to Begin/Stop typing the Process name.");
        mvwprintw(win4,2,2,    "Search Parameter: ");
        wattron (win4, A_REVERSE | A_BOLD);
        mvwprintw(win4,2,20,   "%-32s", charSequence);
        wattroff(win4, A_REVERSE | A_BOLD);
        if (showCredits){
            mvwprintw(win1,0,36,"(Author: Lucca Ribeiro, Bolota :D)");
        }
        wrefresh(win1);
        wrefresh(win2);
        wrefresh(win3);
        wrefresh(win4);
        wrefresh(winInfo);
        wrefresh(winTemp);
        if (breakLoop){
            pthread_join(t1, NULL);
            endwin();
            free(lastClosedProcess);
            free_process_list(listSnapshot, snapshotCount);
            exit(0);
        }
        free(cpuTempSnapshot);
        free_process_list(listSnapshot, snapshotCount); 
        usleep(10000);
    }
}