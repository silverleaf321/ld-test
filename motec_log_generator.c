#include "motec_log_generator.h"
#include <getopt.h>
#include <libgen.h>
#include <sys/stat.h>

#define DEFAULT_FREQUENCY 20.0

static const char* DESCRIPTION = 
    "Generates MoTeC .ld files from external log files generated by: CAN bus dumps, CSV\n"
    "files, or COBB Accessport CSV files";

static const char* EPILOG = 
    "The CAN bus log must be the same format as what is generated by 'candump' with the '-l'\n"
    "option from the linux package can-utils. A MoTeC channel will be created for every signal in the\n"
    "DBC file that has messages in the CAN log. The signal name and units will be directly copied from\n"
    "the DBC file.\n\n"
    "CSV files must have time as their first column. A MoTeC channel will be generated for all remaining\n"
    "columns. All channels will not have any units assigned.\n\n"
    "COBB Accessport CSV logs are simply generated by starting a logging session on the accessport. A\n"
    "MoTeC channel will be created for every channel logged, the name and units will be directly copied\n"
    "over.";

int parse_arguments(int argc, char** argv, GeneratorArgs* args) {
    if (argc < 3) {
        print_usage();
        return -1;
    }

    // defaults
    memset(args, 0, sizeof(GeneratorArgs));
    args->frequency = DEFAULT_FREQUENCY;
    
    static struct option long_options[] = {
        {"output", required_argument, 0, 'o'},
        {"frequency", required_argument, 0, 'f'},
        {"dbc", required_argument, 0, 'd'},
        {"driver", required_argument, 0, 'r'},
        {"vehicle_id", required_argument, 0, 'v'},
        {"vehicle_weight", required_argument, 0, 'w'},
        {"vehicle_type", required_argument, 0, 't'},
        {"vehicle_comment", required_argument, 0, 'c'},
        {"venue_name", required_argument, 0, 'n'},
        {"event_name", required_argument, 0, 'e'},
        {"event_session", required_argument, 0, 's'},
        {"long_comment", required_argument, 0, 'l'},
        {"short_comment", required_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "o:f:d:r:v:w:t:c:n:e:s:l:h:", 
                             long_options, NULL)) != -1) {
        switch (opt) {
            case 'o': args->output_path = strdup(optarg); break;
            case 'f': args->frequency = atof(optarg); break;
            case 'd': args->dbc_path = strdup(optarg); break;
            case 'r': args->driver = strdup(optarg); break;
            case 'v': args->vehicle_id = strdup(optarg); break;
            case 'w': args->vehicle_weight = atoi(optarg); break;
            case 't': args->vehicle_type = strdup(optarg); break;
            case 'c': args->vehicle_comment = strdup(optarg); break;
            case 'n': args->venue_name = strdup(optarg); break;
            case 'e': args->event_name = strdup(optarg); break;
            case 's': args->event_session = strdup(optarg); break;
            case 'l': args->long_comment = strdup(optarg); break;
            case 'h': args->short_comment = strdup(optarg); break;
            default: return -1;
        }
    }

    if (optind + 1 >= argc) {
        print_usage();
        return -1;
    }

    args->log_path = strdup(argv[optind]);
    
    const char* type_str = argv[optind + 1];
    if (strcmp(type_str, "CAN") == 0) {
        args->log_type = LOG_TYPE_CAN;
        if (!args->dbc_path) {
            printf("ERROR: DBC file required for CAN log type\n");
            return -1;
        }
    } else if (strcmp(type_str, "CSV") == 0) {
        args->log_type = LOG_TYPE_CSV;
    } else if (strcmp(type_str, "ACCESSPORT") == 0) {
        args->log_type = LOG_TYPE_ACCESSPORT;
    } else {
        printf("ERROR: Invalid log type: %s\n", type_str);
        return -1;
    }

    return 0;
}

char* get_output_filename(const char* input_path, const char* output_path) {
    if (output_path) {
        char* base = strdup(output_path);
        char* ext = strrchr(base, '.');
        if (ext) *ext = '\0';
        char* result = malloc(strlen(base) + 4);
        sprintf(result, "%s.ld", base);
        free(base);
        return result;
    } else {
        char* base = strdup(input_path);
        char* ext = strrchr(base, '.');
        if (ext) *ext = '\0';
        char* result = malloc(strlen(base) + 4);
        sprintf(result, "%s.ld", base);
        free(base);
        return result;
    }
}

int process_log_file(const GeneratorArgs* args) {
    printf("Loading log...\n");
    
    FILE* f = fopen(args->log_path, "r");
    if (!f) {
        printf("ERROR: Cannot open log file: %s\n", args->log_path);
        return -1;
    }

    DataLog* data_log = datalog_create(""); 
    if (!data_log) {
        fclose(f);
        return -1;
    }

    int result = 0;
    switch (args->log_type) {
        case LOG_TYPE_CAN:
            if (args->dbc_path) {
                printf("Loading DBC...\n");
                result = datalog_from_can_log(data_log, f, args->dbc_path);
            }
            break;
        case LOG_TYPE_CSV:
            result = datalog_from_csv_log(data_log, f);
            break;
        case LOG_TYPE_ACCESSPORT:
            result = datalog_from_accessport_log(data_log, f);
            break;
    }

    fclose(f);

    if (result != 0 || datalog_channel_count(data_log) == 0) {
        printf("ERROR: Failed to find any channels in log data\n");
        // printf("Found %d channels in data_log\n", datalog_channel_count(data_log)); // Debug
        datalog_free(data_log);
        return -1;
    }

    printf("Parsed %.1fs log with %d channels:\n",
       datalog_duration(data_log),  
       datalog_channel_count(data_log));

    data_log_print_channels(data_log);

    printf("Converting to MoTeC log...\n");
    MotecLog* motec_log = motec_log_create();
    if (!motec_log) {
        datalog_free(data_log);
        return -1;
    }

    motec_log_set_metadata(motec_log, 
                          args->driver,
                          args->vehicle_id,
                          args->vehicle_weight,
                          args->vehicle_type,
                          args->vehicle_comment,
                          args->venue_name,
                          args->event_name,
                          args->event_session,
                          args->long_comment,
                          args->short_comment);

    motec_log_initialize(motec_log);
    motec_log_add_all_channels(motec_log, data_log);

    char* output_filename = get_output_filename(args->log_path, args->output_path);
    char* output_dir = dirname(strdup(output_filename));
    
    struct stat st = {0};
    if (stat(output_dir, &st) == -1) {
        printf("Directory '%s' does not exist, will create it\n", output_dir);
        mkdir(output_dir, 0700);
    }

    printf("Saving MoTeC log...\n");
    result = motec_log_write(motec_log, output_filename);

    free(output_filename);
    free(output_dir);
    motec_log_free(motec_log);
    datalog_free(data_log);

    if (result == 0) {
        printf("Done!\n");
    }
    return result;
}

void print_usage(void) {
    printf("%s\n\n", DESCRIPTION);
    printf("Usage: motec_log_generator <log> <log_type> [options]\n");
    printf("Log types: CAN, CSV, ACCESSPORT\n\n");
    printf("Options:\n");
    printf("  --output <file>        Output filename\n");
    printf("  --frequency <hz>       Fixed frequency to resample channels\n");
    printf("  --dbc <file>          DBC file (required for CAN logs)\n");
    printf("  --driver <str>         Driver name\n");
    printf("  --vehicle_id <str>     Vehicle ID\n");
    printf("  --vehicle_weight <n>   Vehicle weight\n");
    printf("  --vehicle_type <str>   Vehicle type\n");
    printf("  --venue_name <str>     Venue name\n");
    printf("  --event_name <str>     Event name\n");
    printf("  --event_session <str>  Event session\n");
    printf("  --long_comment <str>   Long comment\n");
    printf("  --short_comment <str>  Short comment\n\n");
    printf("%s\n", EPILOG);
}

void free_arguments(GeneratorArgs* args) {
    free(args->log_path);
    free(args->output_path);
    free(args->dbc_path);
    free(args->driver);
    free(args->vehicle_id);
    free(args->vehicle_type);
    free(args->vehicle_comment);
    free(args->venue_name);
    free(args->event_name);
    free(args->event_session);
    free(args->long_comment);
    free(args->short_comment);
}

int main(int argc, char** argv) {
    GeneratorArgs args;
    if (parse_arguments(argc, argv, &args) != 0) {
        return 1;
    }

    int result = process_log_file(&args);
    free_arguments(&args);
    return result;
}