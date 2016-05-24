#include <iostream>
#include <libwebsockets.h>
#include <cstring>
#include <map>
#include "jsmn.h"
#include "czmq.h"

// Test sign2

struct per_session_data__dumb_increment {
    int session_number;
};

struct connection_details{
    struct libwebsocket_context *context;
    struct libwebsocket *sock;
};

using namespace std;

static map<int, struct connection_details> sessions;
static unsigned char complete_buffer[2000];
static unsigned char* result_buffer = complete_buffer + LWS_SEND_BUFFER_PRE_PADDING;

static jsmn_parser parser;
static jsmntok_t tokens[256];

static zsock_t* sender;


int callback_dumb_increment(
        struct libwebsocket_context *context,
        struct libwebsocket *sock,
        enum libwebsocket_callback_reasons reason,
        void *user,
        void *in,
        size_t len
){
    switch(reason){
        case LWS_CALLBACK_ADD_POLL_FD:
        case LWS_CALLBACK_PROTOCOL_INIT:
        case LWS_CALLBACK_PROTOCOL_DESTROY:
        case LWS_CALLBACK_DEL_POLL_FD:
            printf("System events\n");
            break;
        case LWS_CALLBACK_FILTER_NETWORK_CONNECTION:
        case LWS_CALLBACK_FILTER_PROTOCOL_CONNECTION:
            printf("Network connection\n");
            break;
        case LWS_CALLBACK_ESTABLISHED:
            printf("Client established\n");
            {
                struct per_session_data__dumb_increment* data = (struct per_session_data__dumb_increment*)user;
                data->session_number = rand();

                sessions[data->session_number] = {context, sock};
            }
            break;
        case LWS_CALLBACK_CLOSED:
            printf("Client closed\n");

            {
                struct per_session_data__dumb_increment* data = (struct per_session_data__dumb_increment*)user;

                sessions.erase(data->session_number);
            }
            break;
        case LWS_CALLBACK_RECEIVE:
        {
            struct per_session_data__dumb_increment* data = (struct per_session_data__dumb_increment*)user;

            char* zmq_msg = (char*)malloc(len + 4);
            *(int*)zmq_msg = data->session_number;

            memcpy(zmq_msg + 4, in, len);

            zframe_t* frame = zframe_new(zmq_msg, len+4);
            zframe_send(&frame, sender, 0);
            /*struct per_session_data__dumb_increment* data = (struct per_session_data__dumb_increment*)user;
            const char* input = (const char*)in;


            jsmn_init(&parser);
            int num_tokens = jsmn_parse(&parser, input, len, tokens, 256);
            int value = 0;

            for(int i = 0 ; i < num_tokens ; i++){
                size_t token_len = (size_t)(tokens[i].end - tokens[i].start);

                if(tokens[i].type == JSMN_STRING && token_len == 3 && strncmp(input + tokens[i].start, "arg", token_len) == 0){
                    // Argument value
                    if(sscanf(input + tokens[i+1].start, "%d", &value) != 1){
                        printf("Bad input\n");
                    }

                }
            }

            data->number += value;



            size_t out_len = (size_t)sprintf((char*)result_buffer, "{\"count\":%d}", data->number);
            printf("data:%s\n", result_buffer);

            libwebsocket_write(sock, result_buffer, out_len, LWS_WRITE_TEXT);
            libwebsocket_callback_on_writable(context, sock);*/

        }
            break;
        default:
            printf("num %d\n", reason);
    }
    return 0;
};

static struct libwebsocket_protocols protocols[] = {
        {
                "fanout-demo",
                callback_dumb_increment,
                sizeof(struct per_session_data__dumb_increment),
                10, /* rx buf size must be >= permessage-deflate rx size */
        },
        NULL
};



int main() {

    struct lws_context_creation_info info;

    info.protocols = protocols;


    info.port = 6789;

    info.ssl_cert_filepath = NULL;
    info.ssl_private_key_filepath = NULL;

    info.gid = -1;
    info.uid = -1;

    info.iface = NULL;
    info.extensions = {NULL};

    libwebsocket_context* context = libwebsocket_create_context(&info);

    zsock_t* rv;

    sender = zsock_new_push("@ipc:///tmp/ws.in");
    rv = zsock_new_pull("@ipc:///tmp/ws.out");

    if (context == NULL) {
        lwsl_err("libwebsocket init failed\n");
        return -1;
    }

    bool force_exit = false;
    int n = 0;

    zsock_t* which;

    zpoller_t* poller = zpoller_new(rv, NULL);

    while (n >= 0 && !force_exit) {
        while((which = (zsock_t*)zpoller_wait(poller, 10)) != NULL){
            zframe_t* frame = zframe_recv(rv);
            void* data = zframe_data(frame);
            size_t size = zframe_size(frame);

            int session = *((int*)data);

            printf("recive zmq message for:%d\n", session);

            auto it = sessions.find(session);
            if(it != sessions.end()){
                memcpy(result_buffer, data + 4, size - 4);

                libwebsocket_write(it->second.sock, result_buffer, size - 4, LWS_WRITE_TEXT);
                libwebsocket_callback_on_writable(it->second.context, it->second.sock);
            }

        }


        n = libwebsocket_service(context, 50);
    }

    return 0;
}