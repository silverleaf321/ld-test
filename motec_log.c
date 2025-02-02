#include "motec_log.h"
#include <string.h>

#define INITIAL_CHANNEL_CAPACITY 1000

MotecLog* motec_log_create(void) {
    MotecLog* log = (MotecLog*)malloc(sizeof(MotecLog));
    if (!log) return NULL;
    
    memset(log, 0, sizeof(MotecLog));
    
    log->channel_capacity = INITIAL_CHANNEL_CAPACITY;
    log->ld_channels = (ldChan**)malloc(sizeof(ldChan*) * log->channel_capacity);
    if (!log->ld_channels) {
        free(log);
        return NULL;
    }
    
    log->channel_count = 0;
    log->datetime = time(NULL);
    
    return log;
}

void motec_log_free(MotecLog* log) {
    if (!log) return;
    
    if (log->ld_channels) {
        for (size_t i = 0; i < log->channel_count; i++) {
            if (log->ld_channels[i]) {
                if (log->ld_channels[i]->data) {
                    free(log->ld_channels[i]->data);
                }
                free(log->ld_channels[i]);
            }
        }
        free(log->ld_channels);
    }
    
    if (log->ld_header) {
        if (log->ld_header->event) {
            if (log->ld_header->event->venue) {
                if (log->ld_header->event->venue->vehicle) {
                    free(log->ld_header->event->venue->vehicle);
                }
                free(log->ld_header->event->venue);
            }
            free(log->ld_header->event);
        }
        free(log->ld_header);
    }
    
    free(log);
}

int motec_log_initialize(MotecLog* log) {
    if (!log) return -1;
    
    // Create vehicle
    ldVehicle* vehicle = (ldVehicle*)malloc(sizeof(ldVehicle));
    if (!vehicle) return -1;
    
    strncpy(vehicle->id, log->vehicle_id, sizeof(vehicle->id)-1);
    vehicle->weight = log->vehicle_weight;
    strncpy(vehicle->type, log->vehicle_type, sizeof(vehicle->type)-1);
    strncpy(vehicle->comment, log->vehicle_comment, sizeof(vehicle->comment)-1);
    
    // Create venue
    ldVenue* venue = (ldVenue*)malloc(sizeof(ldVenue));
    if (!venue) {
        free(vehicle);
        return -1;
    }
    
    strncpy(venue->name, log->venue_name, sizeof(venue->name)-1);
    venue->vehicle_ptr = VEHICLE_PTR;
    venue->vehicle = vehicle;
    
    // Create event
    ldEvent* event = (ldEvent*)malloc(sizeof(ldEvent));
    if (!event) {
        free(venue);
        free(vehicle);
        return -1;
    }
    
    strncpy(event->name, log->event_name, sizeof(event->name)-1);
    strncpy(event->session, log->event_session, sizeof(event->session)-1);
    strncpy(event->comment, log->long_comment, sizeof(event->comment)-1);
    event->venue_ptr = VENUE_PTR;
    event->venue = venue;
    
    // Create header
    log->ld_header = (ldHead*)malloc(sizeof(ldHead));
    if (!log->ld_header) {
        free(event);
        free(venue);
        free(vehicle);
        return -1;
    }
    
    log->ld_header->meta_ptr = HEADER_PTR;
    log->ld_header->data_ptr = HEADER_PTR;
    log->ld_header->event_ptr = EVENT_PTR;
    log->ld_header->event = event;
    
    strncpy(log->ld_header->driver, log->driver, sizeof(log->ld_header->driver)-1);
    strncpy(log->ld_header->vehicleid, log->vehicle_id, sizeof(log->ld_header->vehicleid)-1);
    strncpy(log->ld_header->venue, log->venue_name, sizeof(log->ld_header->venue)-1);
    
    // Convert time_t to struct tm
    struct tm* timeinfo = localtime(&log->datetime);
    if (timeinfo) {
        log->ld_header->datetime = *timeinfo;
    }
    
    strncpy(log->ld_header->short_comment, log->short_comment, sizeof(log->ld_header->short_comment)-1);
    
    return 0;
}

int motec_log_add_channel(MotecLog* log, Channel* channel) {
    if (!log || !channel) return -1;
    
    if (log->channel_count >= log->channel_capacity) {
        size_t new_capacity = log->channel_capacity * 2;
        ldChan** new_channels = (ldChan**)realloc(log->ld_channels, 
            sizeof(ldChan*) * new_capacity);
        if (!new_channels) return -1;
        
        log->ld_channels = new_channels;
        log->channel_capacity = new_capacity;
    }
    
    log->ld_header->data_ptr += sizeof(ldChan);
    
    for (size_t i = 0; i < log->channel_count; i++) {
        log->ld_channels[i]->data_ptr += sizeof(ldChan);
    }
    
    int meta_ptr, prev_meta_ptr, data_ptr;
    if (log->channel_count > 0) {
        meta_ptr = log->ld_channels[log->channel_count-1]->next_meta_ptr;
        prev_meta_ptr = log->ld_channels[log->channel_count-1]->meta_ptr;
        data_ptr = log->ld_channels[log->channel_count-1]->data_ptr + 
            log->ld_channels[log->channel_count-1]->data_len * sizeof(float);
    } else {
        meta_ptr = HEADER_PTR;
        prev_meta_ptr = 0;
        data_ptr = log->ld_header->data_ptr;
    }
    
    ldChan* ld_channel = (ldChan*)malloc(sizeof(ldChan));
    if (!ld_channel) return -1;
    
    ld_channel->meta_ptr = meta_ptr;
    ld_channel->prev_meta_ptr = prev_meta_ptr;
    ld_channel->next_meta_ptr = meta_ptr + sizeof(ldChan);
    ld_channel->data_ptr = data_ptr;
    ld_channel->data_len = channel->message_count;
    ld_channel->dtype = DTYPE_FLOAT32;
    ld_channel->freq = (int)channel_avg_frequency(channel);
    ld_channel->shift = 0;
    ld_channel->mul = 1;
    ld_channel->scale = 1;
    ld_channel->dec = 0;
    strncpy(ld_channel->name, channel->name, sizeof(ld_channel->name)-1);
    strncpy(ld_channel->unit, channel->units, sizeof(ld_channel->unit)-1);
    
    ld_channel->data = malloc(channel->message_count * sizeof(float));
    if (!ld_channel->data) {
        free(ld_channel);
        return -1;
    }
    
    for (size_t i = 0; i < channel->message_count; i++) {
        ((float*)ld_channel->data)[i] = (float)channel->messages[i].value;
    }
    
    log->ld_channels[log->channel_count++] = ld_channel;
    return 0;
}

int motec_log_add_all_channels(MotecLog* log, DataLog* data_log) {
    if (!log || !data_log) return -1;
    
    for (size_t i = 0; i < data_log->channel_count; i++) {
        if (motec_log_add_channel(log, data_log->channels[i]) != 0) {
            return -1;
        }
    }
    return 0;
}

void write_ld_header(ldHead* header, FILE* f, int channel_count) {
    fwrite(&header->meta_ptr, sizeof(uint32_t), 1, f);
    fwrite(&header->data_ptr, sizeof(uint32_t), 1, f);
    fwrite(&header->event_ptr, sizeof(uint32_t), 1, f);
    
    fwrite(header->driver, sizeof(char), 64, f);
    fwrite(header->vehicleid, sizeof(char), 64, f);
    fwrite(header->venue, sizeof(char), 64, f);
    
    fwrite(&header->datetime, sizeof(struct tm), 1, f);
    fwrite(header->short_comment, sizeof(char), 64, f);
    
    if (header->event) {
        fseek(f, header->event_ptr, SEEK_SET);
        fwrite(header->event->name, sizeof(char), 64, f);
        fwrite(header->event->session, sizeof(char), 64, f);
        fwrite(header->event->comment, sizeof(char), 1024, f);
        fwrite(&header->event->venue_ptr, sizeof(uint16_t), 1, f);
        
        if (header->event->venue) {
            fseek(f, header->event->venue_ptr, SEEK_SET);
            fwrite(header->event->venue->name, sizeof(char), 64, f);
            fwrite(&header->event->venue->vehicle_ptr, sizeof(uint16_t), 1, f);
            
            if (header->event->venue->vehicle) {
                fseek(f, header->event->venue->vehicle_ptr, SEEK_SET);
                fwrite(header->event->venue->vehicle->id, sizeof(char), 64, f);
                fwrite(&header->event->venue->vehicle->weight, sizeof(uint32_t), 1, f);
                fwrite(header->event->venue->vehicle->type, sizeof(char), 32, f);
                fwrite(header->event->venue->vehicle->comment, sizeof(char), 32, f);
            }
        }
    }
}

void write_ld_channel(ldChan* channel, FILE* f, int channel_index) {
    uint16_t dtype_a = (channel->dtype == DTYPE_FLOAT32 || channel->dtype == DTYPE_FLOAT16) ? 0x07 : 0x00;
    uint16_t dtype = (channel->dtype == DTYPE_FLOAT16 || channel->dtype == DTYPE_INT16) ? 2 : 4;
    uint16_t magic = 0x2ee1 + channel_index;

    fwrite(&channel->prev_meta_ptr, sizeof(uint32_t), 1, f);
    fwrite(&channel->next_meta_ptr, sizeof(uint32_t), 1, f);
    fwrite(&channel->data_ptr, sizeof(uint32_t), 1, f);
    fwrite(&channel->data_len, sizeof(uint32_t), 1, f);

    fwrite(&magic, sizeof(uint16_t), 1, f);
    fwrite(&dtype_a, sizeof(uint16_t), 1, f);
    fwrite(&dtype, sizeof(uint16_t), 1, f);
    fwrite(&channel->freq, sizeof(int16_t), 1, f);
    fwrite(&channel->shift, sizeof(int16_t), 1, f);
    fwrite(&channel->mul, sizeof(int16_t), 1, f);
    fwrite(&channel->scale, sizeof(int16_t), 1, f);
    fwrite(&channel->dec, sizeof(int16_t), 1, f);

    fwrite(channel->name, sizeof(char), 32, f);
    fwrite(channel->short_name, sizeof(char), 8, f);
    fwrite(channel->unit, sizeof(char), 12, f);
}

int motec_log_write(MotecLog* log, const char* filename) {
    if (!log || !filename) return -1;
    
    FILE* f = fopen(filename, "wb");
    if (!f) return -1;
    
    if (log->channel_count > 0) {
        log->ld_channels[log->channel_count-1]->next_meta_ptr = 0;
        
        write_ld_header(log->ld_header, f, log->channel_count);
        
        for (size_t i = 0; i < log->channel_count; i++) {
            ldChan* chan = log->ld_channels[i];
            fseek(f, chan->meta_ptr, SEEK_SET);
            write_ld_channel(chan, f, i);
            
            fseek(f, chan->data_ptr, SEEK_SET);
            fwrite(chan->data, sizeof(float), chan->data_len, f);
        }
    } else {
        write_ld_header(log->ld_header, f, 0);
    }
    
    fclose(f);
    return 0;
}

void motec_log_set_metadata(MotecLog* log,
                           const char* driver,
                           const char* vehicle_id, 
                           unsigned int vehicle_weight,
                           const char* vehicle_type,
                           const char* vehicle_comment,
                           const char* venue_name,
                           const char* event_name,
                           const char* event_session,
                           const char* long_comment,
                           const char* short_comment) {
    
    if (driver) strncpy(log->driver, driver, sizeof(log->driver)-1);
    if (vehicle_id) strncpy(log->vehicle_id, vehicle_id, sizeof(log->vehicle_id)-1);
    log->vehicle_weight = vehicle_weight;
    if (vehicle_type) strncpy(log->vehicle_type, vehicle_type, sizeof(log->vehicle_type)-1);
    if (vehicle_comment) strncpy(log->vehicle_comment, vehicle_comment, sizeof(log->vehicle_comment)-1);
    if (venue_name) strncpy(log->venue_name, venue_name, sizeof(log->venue_name)-1);
    if (event_name) strncpy(log->event_name, event_name, sizeof(log->event_name)-1);
    if (event_session) strncpy(log->event_session, event_session, sizeof(log->event_session)-1);
    if (long_comment) strncpy(log->long_comment, long_comment, sizeof(log->long_comment)-1);
    if (short_comment) strncpy(log->short_comment, short_comment, sizeof(log->short_comment)-1);
}