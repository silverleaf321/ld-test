#include "ldparser.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Helper function to decode strings
char* decode_string(const char* bytes, size_t len) {
    char* result = malloc(len + 1);
    if (!result) return NULL;
    
    memcpy(result, bytes, len);
    result[len] = '\0';
    
    // Trim trailing nulls and whitespace
    size_t end = len;
    while (end > 0 && (result[end-1] == '\0' || result[end-1] == ' '))
        end--;
    
    result[end] = '\0';
    return result;
}

// Read vehicle information
ldVehicle* read_vehicle(FILE* f) {
    ldVehicle* vehicle = malloc(sizeof(ldVehicle));
    if (!vehicle) return NULL;
    
    char id[64];
    uint32_t weight;
    char type[32];
    char comment[32];
    
    if (fread(id, 64, 1, f) != 1 ||
        fread(&weight, 4, 1, f) != 1 ||
        fread(type, 32, 1, f) != 1 ||
        fread(comment, 32, 1, f) != 1) {
        free(vehicle);
        return NULL;
    }
    
    memcpy(vehicle->id, id, 64);
    vehicle->weight = weight;
    memcpy(vehicle->type, type, 32);
    memcpy(vehicle->comment, comment, 32);
    
    return vehicle;
}

// Read venue information
ldVenue* read_venue(FILE* f) {
    ldVenue* venue = malloc(sizeof(ldVenue));
    if (!venue) return NULL;
    
    char name[64];
    uint16_t vehicle_ptr;
    
    if (fread(name, 64, 1, f) != 1 ||
        fseek(f, 1034, SEEK_CUR) != 0 ||  // Skip 1034 bytes
        fread(&vehicle_ptr, 2, 1, f) != 1) {
        free(venue);
        return NULL;
    }
    
    memcpy(venue->name, name, 64);
    venue->vehicle_ptr = vehicle_ptr;
    
    if (vehicle_ptr > 0) {
        long pos = ftell(f);
        fseek(f, vehicle_ptr, SEEK_SET);
        venue->vehicle = read_vehicle(f);
        fseek(f, pos, SEEK_SET);
    } else {
        venue->vehicle = NULL;
    }
    
    return venue;
}

// Read event information
ldEvent* read_event(FILE* f) {
    ldEvent* event = malloc(sizeof(ldEvent));
    if (!event) return NULL;
    
    char name[64];
    char session[64];
    char comment[1024];
    uint16_t venue_ptr;
    
    if (fread(name, 64, 1, f) != 1 ||
        fread(session, 64, 1, f) != 1 ||
        fread(comment, 1024, 1, f) != 1 ||
        fread(&venue_ptr, 2, 1, f) != 1) {
        free(event);
        return NULL;
    }
    
    memcpy(event->name, name, 64);
    memcpy(event->session, session, 64);
    memcpy(event->comment, comment, 1024);
    event->venue_ptr = venue_ptr;
    
    if (venue_ptr > 0) {
        long pos = ftell(f);
        fseek(f, venue_ptr, SEEK_SET);
        event->venue = read_venue(f);
        fseek(f, pos, SEEK_SET);
    } else {
        event->venue = NULL;
    }
    
    return event;
}

// Main function to read an LD file
ldData* read_ldfile(const char* filename) {
    FILE* f = fopen(filename, "rb");
    if (!f) return NULL;
    
    ldData* data = malloc(sizeof(ldData));
    if (!data) {
        fclose(f);
        return NULL;
    }
    
    // Read header
    data->head = read_head(f);
    if (!data->head) {
        free(data);
        fclose(f);
        return NULL;
    }
    
    // Read channels
    data->channs = read_channels(filename, data->head->meta_ptr, &data->chann_count);
    if (!data->channs) {
        free(data->head);
        free(data);
        fclose(f);
        return NULL;
    }
    
    fclose(f);
    return data;
}

// Free allocated memory
void free_lddata(ldData* data) {
    if (!data) return;
    
    if (data->head) {
        if (data->head->event) {
            if (data->head->event->venue) {
                if (data->head->event->venue->vehicle) {
                    free(data->head->event->venue->vehicle);
                }
                free(data->head->event->venue);
            }
            free(data->head->event);
        }
        free(data->head);
    }
    
    if (data->channs) {
        for (size_t i = 0; i < data->chann_count; i++) {
            if (data->channs[i]) {
                free(data->channs[i]->data);
                free(data->channs[i]->file_path);
                free(data->channs[i]);
            }
        }
        free(data->channs);
    }
    
    free(data);
}
// Add to ldparser.c:

ldHead* read_head(FILE* f) {
    ldHead* head = malloc(sizeof(ldHead));
    if (!head) return NULL;

    // Read marker and validate
    uint32_t ldmarker;
    if (fread(&ldmarker, sizeof(uint32_t), 1, f) != 1) {
        free(head);
        return NULL;
    }
    fseek(f, 4, SEEK_CUR); // Skip 4x padding

    // Read pointers
    if (fread(&head->meta_ptr, sizeof(uint32_t), 1, f) != 1 ||
        fread(&head->data_ptr, sizeof(uint32_t), 1, f) != 1) {
        free(head);
        return NULL;
    }

    fseek(f, 20, SEEK_CUR); // Skip 20x padding
    
    uint32_t event_ptr;
    if (fread(&event_ptr, sizeof(uint32_t), 1, f) != 1) {
        free(head);
        return NULL;
    }

    fseek(f, 24, SEEK_CUR); // Skip 24x padding

    // Skip static numbers
    uint16_t static_nums[3];
    fseek(f, sizeof(uint16_t) * 3, SEEK_CUR);

    // Skip device info
    fseek(f, 4 + 8 + 2 + 2, SEEK_CUR);

    // Skip num_channs
    uint32_t num_channs;
    fseek(f, 4, SEEK_CUR);
    
    fseek(f, 4, SEEK_CUR); // Skip 4x padding

    // Read date/time
    char date[16], time[16];
    if (fread(date, 16, 1, f) != 1) {
        free(head);
        return NULL;
    }
    fseek(f, 16, SEEK_CUR); // Skip 16x padding
    
    if (fread(time, 16, 1, f) != 1) {
        free(head);
        return NULL;
    }
    fseek(f, 16, SEEK_CUR); // Skip 16x padding

    // Read strings
    char driver[64], vehicleid[64], venue[64];
    if (fread(driver, 64, 1, f) != 1 ||
        fread(vehicleid, 64, 1, f) != 1) {
        free(head);
        return NULL;
    }
    
    fseek(f, 64, SEEK_CUR); // Skip 64x padding
    
    if (fread(venue, 64, 1, f) != 1) {
        free(head);
        return NULL;
    }

    fseek(f, 64, SEEK_CUR); // Skip 64x padding
    fseek(f, 1024, SEEK_CUR); // Skip 1024x padding

    // Skip pro logging
    fseek(f, 4, SEEK_CUR);
    fseek(f, 66, SEEK_CUR); // Skip 66x padding

    // Read comments and event info
    char short_comment[64], event[64], session[64];
    if (fread(short_comment, 64, 1, f) != 1) {
        free(head);
        return NULL;
    }
    
    fseek(f, 126, SEEK_CUR); // Skip 126x padding
    
    if (fread(event, 64, 1, f) != 1 ||
        fread(session, 64, 1, f) != 1) {
        free(head);
        return NULL;
    }

    // Copy strings
    strncpy(head->driver, decode_string(driver, 64), 64);
    strncpy(head->vehicleid, decode_string(vehicleid, 64), 64);
    strncpy(head->venue, decode_string(venue, 64), 64);
    strncpy(head->short_comment, decode_string(short_comment, 64), 64);

    // Parse date/time
    char datetime_str[33];
    snprintf(datetime_str, sizeof(datetime_str), "%s %s", 
             decode_string(date, 16), decode_string(time, 16));
    strptime(datetime_str, "%d/%m/%Y %H:%M:%S", &head->datetime);

    // Read auxiliary event data if present
    head->aux_ptr = event_ptr;
    if (event_ptr > 0) {
        fseek(f, event_ptr, SEEK_SET);
        head->aux = read_event(f);
    } else {
        head->aux = NULL;
    }

    return head;
}

ldChan** read_channels(const char* filename, uint32_t meta_ptr, size_t* count) {
    FILE* f = fopen(filename, "rb");
    if (!f) return NULL;

    // Count channels first
    size_t num_channels = 0;
    uint32_t current_ptr = meta_ptr;
    
    while (current_ptr) {
        fseek(f, current_ptr, SEEK_SET);
        
        uint32_t next_ptr;
        if (fread(&next_ptr, sizeof(uint32_t), 1, f) != 1) {
            fclose(f);
            return NULL;
        }
        
        num_channels++;
        current_ptr = next_ptr;
    }

    // Allocate channel array
    ldChan** channels = malloc(sizeof(ldChan*) * num_channels);
    if (!channels) {
        fclose(f);
        return NULL;
    }

    // Read each channel
    current_ptr = meta_ptr;
    size_t idx = 0;
    
    while (current_ptr && idx < num_channels) {
        fseek(f, current_ptr, SEEK_SET);
        
        ldChan* chan = malloc(sizeof(ldChan));
        if (!chan) {
            // Cleanup on error
            for (size_t i = 0; i < idx; i++) {
                free(channels[i]);
            }
            free(channels);
            fclose(f);
            return NULL;
        }

        chan->file_path = strdup(filename);
        chan->meta_ptr = current_ptr;

        // Read channel metadata
        if (fread(&chan->prev_meta_ptr, sizeof(uint32_t), 1, f) != 1 ||
            fread(&chan->next_meta_ptr, sizeof(uint32_t), 1, f) != 1 ||
            fread(&chan->data_ptr, sizeof(uint32_t), 1, f) != 1 ||
            fread(&chan->data_len, sizeof(uint32_t), 1, f) != 1) {
            free(chan);
            for (size_t i = 0; i < idx; i++) {
                free(channels[i]);
            }
            free(channels);
            fclose(f);
            return NULL;
        }

        // Skip counter
        fseek(f, 2, SEEK_CUR);

        // Read data type info
        uint16_t dtype_a, dtype;
        if (fread(&dtype_a, sizeof(uint16_t), 1, f) != 1 ||
            fread(&dtype, sizeof(uint16_t), 1, f) != 1) {
            free(chan);
            for (size_t i = 0; i < idx; i++) {
                free(channels[i]);
            }
            free(channels);
            fclose(f);
            return NULL;
        }

        // Set data type based on dtype_a and dtype
        if (dtype_a == 0x07) {
            chan->dtype = (dtype == 2) ? DTYPE_FLOAT16 : DTYPE_FLOAT32;
        } else {
            chan->dtype = (dtype == 2) ? DTYPE_INT16 : DTYPE_INT32;
        }

        // Read remaining metadata
        if (fread(&chan->freq, sizeof(uint16_t), 1, f) != 1 ||
            fread(&chan->shift, sizeof(int16_t), 1, f) != 1 ||
            fread(&chan->mul, sizeof(int16_t), 1, f) != 1 ||
            fread(&chan->scale, sizeof(int16_t), 1, f) != 1 ||
            fread(&chan->dec, sizeof(int16_t), 1, f) != 1) {
            free(chan);
            for (size_t i = 0; i < idx; i++) {
                free(channels[i]);
            }
            free(channels);
            fclose(f);
            return NULL;
        }

        // Read strings
        char name[32], short_name[8], unit[12];
        if (fread(name, 32, 1, f) != 1 ||
            fread(short_name, 8, 1, f) != 1 ||
            fread(unit, 12, 1, f) != 1) {
            free(chan);
            for (size_t i = 0; i < idx; i++) {
                free(channels[i]);
            }
            free(channels);
            fclose(f);
            return NULL;
        }

        // Copy strings
        strncpy(chan->name, decode_string(name, 32), 32);
        strncpy(chan->short_name, decode_string(short_name, 8), 8);
        strncpy(chan->unit, decode_string(unit, 12), 12);

        // Initialize data pointer
        chan->data = NULL;

        channels[idx++] = chan;
        current_ptr = chan->next_meta_ptr;
    }

    *count = num_channels;
    fclose(f);
    return channels;
}