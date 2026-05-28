#ifndef EMSCRIPTEN

#include "websocket.h"
#include "logger/logger.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

/* ---------- platform sockets ------------------------------------------- */

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  pragma comment(lib, "ws2_32.lib")
typedef SOCKET ws_sock_t;
#  define WS_INVALID_SOCK    INVALID_SOCKET
#  define ws_sock_close(s)   closesocket(s)
#  define ws_sock_errno()    WSAGetLastError()
#  define WS_EINTR           WSAEINTR
#  define WS_SHUT_RDWR       SD_BOTH
static int ws_init_sockets(void) { WSADATA d; return WSAStartup(MAKEWORD(2,2),&d)==0?0:-1; }
#  define ws_cleanup_sockets() WSACleanup()
#else
#  include <arpa/inet.h>
#  include <errno.h>
#  include <fcntl.h>
#  include <netdb.h>
#  include <netinet/in.h>
#  include <sys/select.h>
#  include <sys/socket.h>
#  include <unistd.h>
typedef int ws_sock_t;
#  define WS_INVALID_SOCK    (-1)
#  define ws_sock_close(s)   close(s)
#  define ws_sock_errno()    errno
#  define WS_EINTR           EINTR
#  define WS_SHUT_RDWR       SHUT_RDWR
#  define ws_init_sockets()      (0)
#  define ws_cleanup_sockets()   ((void)0)
#endif

/* ---------- SHA-1 (vendored, for WebSocket handshake key) --------------- */

typedef struct { uint32_t st[5]; uint32_t cnt[2]; uint8_t buf[64]; } sha1_ctx_t;

#define SHA1_ROL(x,n) (((x)<<(n))|((x)>>(32-(n))))

static void sha1_transform(uint32_t st[5], const uint8_t blk[64])
{
    uint32_t a,b,c,d,e,w[80],t; int i;
    for (i=0;i<16;i++)
        w[i]=((uint32_t)blk[i*4]<<24)|((uint32_t)blk[i*4+1]<<16)|
             ((uint32_t)blk[i*4+2]<<8)|(uint32_t)blk[i*4+3];
    for (i=16;i<80;i++) w[i]=SHA1_ROL(w[i-3]^w[i-8]^w[i-14]^w[i-16],1);
    a=st[0];b=st[1];c=st[2];d=st[3];e=st[4];
    for(i= 0;i<20;i++){t=SHA1_ROL(a,5)+((b&c)|(~b&d))+e+w[i]+0x5A827999u;e=d;d=c;c=SHA1_ROL(b,30);b=a;a=t;}
    for(i=20;i<40;i++){t=SHA1_ROL(a,5)+(b^c^d)+e+w[i]+0x6ED9EBA1u;e=d;d=c;c=SHA1_ROL(b,30);b=a;a=t;}
    for(i=40;i<60;i++){t=SHA1_ROL(a,5)+((b&c)|(b&d)|(c&d))+e+w[i]+0x8F1BBCDCu;e=d;d=c;c=SHA1_ROL(b,30);b=a;a=t;}
    for(i=60;i<80;i++){t=SHA1_ROL(a,5)+(b^c^d)+e+w[i]+0xCA62C1D6u;e=d;d=c;c=SHA1_ROL(b,30);b=a;a=t;}
    st[0]+=a;st[1]+=b;st[2]+=c;st[3]+=d;st[4]+=e;
}
static void sha1_init(sha1_ctx_t *c){
    c->st[0]=0x67452301u;c->st[1]=0xEFCDAB89u;c->st[2]=0x98BADCFEu;
    c->st[3]=0x10325476u;c->st[4]=0xC3D2E1F0u;c->cnt[0]=c->cnt[1]=0;
}
static void sha1_update(sha1_ctx_t *c, const uint8_t *d, size_t n)
{
    uint32_t j=(c->cnt[0]>>3)&63; size_t i;
    if((c->cnt[0]+=(uint32_t)(n<<3))<(uint32_t)(n<<3))c->cnt[1]++;
    c->cnt[1]+=(uint32_t)(n>>29);
    if((j+n)>63){memcpy(&c->buf[j],d,(i=64-j));sha1_transform(c->st,c->buf);
        for(;i+63<n;i+=64)sha1_transform(c->st,&d[i]);j=0;}else i=0;
    memcpy(&c->buf[j],&d[i],n-i);
}
static void sha1_final(sha1_ctx_t *c, uint8_t out[20])
{
    uint8_t fc[8],ch; int i;
    for(i=0;i<8;i++)fc[i]=(uint8_t)((c->cnt[(i>=4?0:1)]>>((3-(i&3))*8))&0xff);
    ch=0x80;sha1_update(c,&ch,1);
    while((c->cnt[0]&504)!=448){ch=0;sha1_update(c,&ch,1);}
    sha1_update(c,fc,8);
    for(i=0;i<20;i++)out[i]=(uint8_t)((c->st[i>>2]>>((3-(i&3))*8))&0xff);
}

/* ---------- Base64 encode ----------------------------------------------- */

static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static void b64_encode(const uint8_t *src, size_t n, char *out)
{
    size_t i; char *p=out;
    for(i=0;i+2<n;i+=3){
        *p++=b64[src[i]>>2];*p++=b64[((src[i]&3)<<4)|(src[i+1]>>4)];
        *p++=b64[((src[i+1]&0xf)<<2)|(src[i+2]>>6)];*p++=b64[src[i+2]&0x3f];
    }
    if(i<n){
        *p++=b64[src[i]>>2];
        if(i+1<n){*p++=b64[((src[i]&3)<<4)|(src[i+1]>>4)];*p++=b64[(src[i+1]&0xf)<<2];}
        else{*p++=b64[(src[i]&3)<<4];*p++='=';}
        *p++='=';
    }
    *p='\0';
}

/* ---------- URL parser -------------------------------------------------- */

typedef struct { char host[256]; char path[256]; int port; } ws_url_t;

static bool ws_parse_url(const char *url, ws_url_t *out)
{
    const char *p; const char *slash; char host_port[256]; char *colon; size_t hlen;
    memset(out,0,sizeof(*out)); out->port=80; snprintf(out->path,sizeof(out->path),"/");
    if(strncmp(url,"ws://",5)==0){p=url+5;}
    else if(strncmp(url,"wss://",6)==0){p=url+6;out->port=443;}
    else return false;
    slash=strchr(p,'/');
    if(slash){hlen=(size_t)(slash-p);snprintf(out->path,sizeof(out->path),"%s",slash);}
    else hlen=strlen(p);
    if(hlen>=sizeof(host_port))hlen=sizeof(host_port)-1;
    memcpy(host_port,p,hlen);host_port[hlen]='\0';
    colon=strrchr(host_port,':');
    if(colon){*colon='\0';out->port=atoi(colon+1);}
    snprintf(out->host,sizeof(out->host),"%s",host_port);
    return out->host[0]!='\0';
}

/* ---------- RFC 6455 constants ------------------------------------------ */

#define WS_FIN  0x80u
#define WS_MASK 0x80u
#define WS_OP_CONTINUATION 0x0
#define WS_OP_TEXT         0x1
#define WS_OP_BINARY       0x2
#define WS_OP_CLOSE        0x8
#define WS_OP_PING         0x9
#define WS_OP_PONG         0xA

/* ---------- Internal types ---------------------------------------------- */

#define WEBSOCKET_DEFAULT_CLOSE_CODE 1000

typedef enum {
    WEBSOCKET_EVENT_OPEN    = 1,
    WEBSOCKET_EVENT_CLOSE   = 2,
    WEBSOCKET_EVENT_ERROR   = 3,
    WEBSOCKET_EVENT_MESSAGE = 4
} websocket_event_type_t;

typedef struct websocket_event {
    websocket_event_type_t type;
    int   code;
    bool  is_text;
    int   len;
    char  reason[128];
    char *message;
    struct websocket_event *next;
} websocket_event_t;

struct websocket {
    ws_sock_t        sockfd;
    pthread_t        worker;
    pthread_mutex_t  mutex;
    char             url[WEBSOCKET_MAX_URL_LENGTH];
    char             requested_close_reason[128];
    bool             connected;
    bool             worker_started;
    bool             close_requested;
    bool             destroy_requested;
    bool             close_emitted;
    bool             error_emitted;
    int              requested_close_code;
    websocket_callbacks_t callbacks;
    void            *user_data;
    websocket_event_t *event_head;
    websocket_event_t *event_tail;
};

/* ---------- Event queue (all callers hold mutex) ------------------------- */

static void ws_enqueue_locked(websocket_t *ws, websocket_event_type_t type,
                               int code, const char *reason,
                               const char *msg, int len, bool is_text)
{
    websocket_event_t *ev=(websocket_event_t *)calloc(1,sizeof(*ev));
    if(!ev)return;
    ev->type=type;ev->code=code;ev->is_text=is_text;ev->len=len;
    if(reason)snprintf(ev->reason,sizeof(ev->reason),"%s",reason);
    if(msg&&len>0){
        ev->message=(char *)malloc((size_t)len);
        if(!ev->message){free(ev);return;}
        memcpy(ev->message,msg,(size_t)len);
    }
    if(!ws->event_tail){ws->event_head=ws->event_tail=ev;}
    else{ws->event_tail->next=ev;ws->event_tail=ev;}
}

static void ws_enqueue_error_locked(websocket_t *ws)
{
    if(!ws||ws->error_emitted)return;
    ws->error_emitted=true;
    ws_enqueue_locked(ws,WEBSOCKET_EVENT_ERROR,0,NULL,NULL,0,false);
}

static void ws_enqueue_close_locked(websocket_t *ws, int code, const char *reason)
{
    if(!ws||ws->close_emitted)return;
    ws->close_emitted=true; ws->connected=false;
    ws_enqueue_locked(ws,WEBSOCKET_EVENT_CLOSE,code,reason,NULL,0,false);
}

/* ---------- Socket I/O helpers ------------------------------------------ */

static int ws_recv_all(ws_sock_t fd, uint8_t *buf, size_t n)
{
    size_t done=0;
    while(done<n){
        int r=(int)recv(fd,(char *)(buf+done),(int)(n-done),0);
        if(r<=0)return -1;
        done+=(size_t)r;
    }
    return 0;
}

static int ws_send_all(ws_sock_t fd, const uint8_t *buf, size_t n)
{
    size_t done=0;
    while(done<n){
        int r=(int)send(fd,(const char *)(buf+done),(int)(n-done),0);
        if(r<=0)return -1;
        done+=(size_t)r;
    }
    return 0;
}

/* ---------- Frame encoder (caller holds mutex) -------------------------- */

static uint32_t ws_xorshift(void)
{
    static uint32_t s=0;
    if(!s)s=(uint32_t)time(NULL)^0xA5A5A5A5u;
    s^=s<<13;s^=s>>17;s^=s<<5;
    return s;
}

static int ws_send_frame_locked(websocket_t *ws, const void *data, size_t len,
                                  uint8_t opcode)
{
    uint8_t hdr[14]; size_t hlen=0;
    uint32_t mk; uint8_t mkb[4]; uint8_t *masked; size_t i;

    if(!ws||ws->sockfd==WS_INVALID_SOCK||!ws->connected)return -1;

    hdr[hlen++]=(uint8_t)(WS_FIN|(opcode&0x0f));
    if(len<=125){
        hdr[hlen++]=(uint8_t)(WS_MASK|len);
    } else if(len<=0xFFFFu){
        hdr[hlen++]=WS_MASK|126;
        hdr[hlen++]=(uint8_t)(len>>8);hdr[hlen++]=(uint8_t)(len&0xff);
    } else {
        hdr[hlen++]=WS_MASK|127;
        for(i=8;i>0;i--)hdr[hlen++]=(uint8_t)((len>>((i-1)*8))&0xff);
    }

    mk=ws_xorshift();
    mkb[0]=(uint8_t)(mk>>24);mkb[1]=(uint8_t)(mk>>16);
    mkb[2]=(uint8_t)(mk>>8); mkb[3]=(uint8_t)(mk&0xff);
    memcpy(hdr+hlen,mkb,4);hlen+=4;

    if(ws_send_all(ws->sockfd,hdr,hlen)!=0)return -1;
    if(len==0)return 0;

    masked=(uint8_t *)malloc(len);
    if(!masked)return -1;
    for(i=0;i<len;i++)masked[i]=((const uint8_t *)data)[i]^mkb[i&3];
    int rc=ws_send_all(ws->sockfd,masked,len);
    free(masked);
    return rc;
}

/* Send a close frame with code+reason payload */
static void ws_send_close_frame_locked(websocket_t *ws, int code, const char *reason)
{
    uint8_t payload[127]; size_t plen=0;
    uint16_t nc=htons((uint16_t)(code>0?code:WEBSOCKET_DEFAULT_CLOSE_CODE));
    memcpy(payload,&nc,2);plen=2;
    if(reason&&reason[0]){
        size_t rlen=strlen(reason);
        if(rlen>123)rlen=123;
        memcpy(payload+plen,reason,rlen);plen+=rlen;
    }
    ws_send_frame_locked(ws,payload,plen,WS_OP_CLOSE);
}

/* ---------- HTTP/1.1 WebSocket upgrade handshake ----------------------- */

static bool ws_do_handshake(ws_sock_t fd, const ws_url_t *url)
{
    uint8_t raw[16]; char ws_key[32]; char request[1024];
    char response[4096]; char expected[64];
    uint8_t sha_in[64],sha_out[20]; sha1_ctx_t sha;
    const char *magic="258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    size_t klen,mlen; int n,total=0;

    /* pseudo-random 16-byte key */
    {uint32_t s=(uint32_t)time(NULL)^0xDEADBEEFu;
     for(int i=0;i<4;i++){s^=s<<13;s^=s>>17;s^=s<<5;memcpy(raw+i*4,&s,4);}}
    b64_encode(raw,16,ws_key);

    /* expected Sec-WebSocket-Accept */
    klen=strlen(ws_key);mlen=strlen(magic);
    memcpy(sha_in,ws_key,klen);memcpy(sha_in+klen,magic,mlen);
    sha1_init(&sha);sha1_update(&sha,sha_in,klen+mlen);sha1_final(&sha,sha_out);
    b64_encode(sha_out,20,expected);

    n=snprintf(request,sizeof(request),
        "GET %s HTTP/1.1\r\n"
        "Host: %s:%d\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: %s\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "\r\n",
        url->path,url->host,url->port,ws_key);
    if(ws_send_all(fd,(const uint8_t *)request,(size_t)n)!=0)return false;

    /* read headers byte-by-byte until \r\n\r\n */
    while(total<(int)sizeof(response)-1){
        n=(int)recv(fd,response+total,1,0);
        if(n<=0)return false;
        total++;
        if(total>=4&&response[total-4]=='\r'&&response[total-3]=='\n'&&
           response[total-2]=='\r'&&response[total-1]=='\n')break;
    }
    response[total]='\0';

    if(!strstr(response,"101"))return false;

    char *ah=strstr(response,"Sec-WebSocket-Accept:");
    if(ah){
        ah+=21;while(*ah==' ')ah++;
        char *end=strstr(ah,"\r\n");
        char actual[64]={0};
        if(end){size_t al=(size_t)(end-ah);if(al>=sizeof(actual))al=sizeof(actual)-1;memcpy(actual,ah,al);}
        if(strcmp(actual,expected)!=0)return false;
    }
    return true;
}

/* ---------- Worker thread ----------------------------------------------- */

static void *websocket_worker_main(void *arg)
{
    websocket_t *ws=(websocket_t *)arg;
    ws_url_t url;
    ws_sock_t fd=WS_INVALID_SOCK;
    struct addrinfo hints,*res=NULL;
    char port_str[16];

    if(!ws_parse_url(ws->url,&url)){
        pthread_mutex_lock(&ws->mutex);
        ws_enqueue_error_locked(ws);
        ws_enqueue_close_locked(ws,1006,"invalid url");
        pthread_mutex_unlock(&ws->mutex);
        return NULL;
    }

    memset(&hints,0,sizeof(hints));
    hints.ai_family=AF_UNSPEC;hints.ai_socktype=SOCK_STREAM;
    snprintf(port_str,sizeof(port_str),"%d",url.port);

    if(getaddrinfo(url.host,port_str,&hints,&res)!=0||!res){
        log_error("WEBSOCKET: DNS failed for %s",url.host);
        pthread_mutex_lock(&ws->mutex);
        ws_enqueue_error_locked(ws);ws_enqueue_close_locked(ws,1006,"dns failed");
        pthread_mutex_unlock(&ws->mutex);
        return NULL;
    }

    fd=socket(res->ai_family,res->ai_socktype,res->ai_protocol);
    if(fd==WS_INVALID_SOCK){
        freeaddrinfo(res);
        pthread_mutex_lock(&ws->mutex);
        ws_enqueue_error_locked(ws);ws_enqueue_close_locked(ws,1006,"socket failed");
        pthread_mutex_unlock(&ws->mutex);
        return NULL;
    }

    if(connect(fd,res->ai_addr,(int)res->ai_addrlen)!=0){
        freeaddrinfo(res);ws_sock_close(fd);
        log_error("WEBSOCKET: connect failed for %s:%d",url.host,url.port);
        pthread_mutex_lock(&ws->mutex);
        ws_enqueue_error_locked(ws);ws_enqueue_close_locked(ws,1006,"connect failed");
        pthread_mutex_unlock(&ws->mutex);
        return NULL;
    }
    freeaddrinfo(res);

    if(!ws_do_handshake(fd,&url)){
        ws_sock_close(fd);
        log_error("WEBSOCKET: handshake failed for %s",ws->url);
        pthread_mutex_lock(&ws->mutex);
        ws_enqueue_error_locked(ws);ws_enqueue_close_locked(ws,1006,"handshake failed");
        pthread_mutex_unlock(&ws->mutex);
        return NULL;
    }

    pthread_mutex_lock(&ws->mutex);
    ws->sockfd=fd;ws->connected=true;
    ws_enqueue_locked(ws,WEBSOCKET_EVENT_OPEN,0,NULL,NULL,0,false);
    if(ws->close_requested)
        ws_send_close_frame_locked(ws,ws->requested_close_code,ws->requested_close_reason);
    pthread_mutex_unlock(&ws->mutex);
    log_debug("WEBSOCKET: Connected to %s",ws->url);

    /* reassembly buffer for fragmented messages */
    uint8_t *frag=NULL; size_t frag_len=0,frag_cap=0; bool frag_text=false;

    for(;;){
        uint8_t hdr[2];

        pthread_mutex_lock(&ws->mutex);
        if(ws->destroy_requested){pthread_mutex_unlock(&ws->mutex);break;}
        pthread_mutex_unlock(&ws->mutex);

        /* wait for data */
        {
            fd_set rfds;FD_ZERO(&rfds);FD_SET(fd,&rfds);
            int sel=select((int)fd+1,&rfds,NULL,NULL,NULL);
            if(sel<=0){
                if(ws_sock_errno()==WS_EINTR)continue;
                pthread_mutex_lock(&ws->mutex);
                ws_enqueue_error_locked(ws);ws_enqueue_close_locked(ws,1006,"select failed");
                pthread_mutex_unlock(&ws->mutex);
                break;
            }
        }

        if(ws_recv_all(fd,hdr,2)!=0){
            pthread_mutex_lock(&ws->mutex);
            if(!ws->close_emitted)
                ws_enqueue_close_locked(ws,
                    ws->close_requested?ws->requested_close_code:1006,
                    ws->close_requested?ws->requested_close_reason:"connection closed");
            pthread_mutex_unlock(&ws->mutex);
            break;
        }

        bool  fin    =(hdr[0]&0x80)!=0;
        uint8_t op   = hdr[0]&0x0f;
        bool  masked =(hdr[1]&0x80)!=0;
        uint64_t plen= hdr[1]&0x7fu;

        if(plen==126){
            uint8_t ext[2];if(ws_recv_all(fd,ext,2)!=0)goto recv_error;
            plen=((uint64_t)ext[0]<<8)|ext[1];
        } else if(plen==127){
            uint8_t ext[8];if(ws_recv_all(fd,ext,8)!=0)goto recv_error;
            plen=0;for(int i=0;i<8;i++)plen=(plen<<8)|ext[i];
        }

        uint8_t mk[4]={0};
        if(masked&&ws_recv_all(fd,mk,4)!=0)goto recv_error;

        uint8_t *payload=NULL;
        if(plen>0){
            if(plen>16u*1024u*1024u)goto recv_error; /* 16 MB sanity cap */
            payload=(uint8_t *)malloc((size_t)plen);
            if(!payload)goto recv_error;
            if(ws_recv_all(fd,payload,(size_t)plen)!=0){free(payload);goto recv_error;}
            if(masked)for(size_t i=0;i<(size_t)plen;i++)payload[i]^=mk[i&3];
        }

        /* control frames */
        if(op==WS_OP_PING){
            pthread_mutex_lock(&ws->mutex);
            ws_send_frame_locked(ws,payload,(size_t)plen,WS_OP_PONG);
            pthread_mutex_unlock(&ws->mutex);
            free(payload);continue;
        }
        if(op==WS_OP_PONG){free(payload);continue;}
        if(op==WS_OP_CLOSE){
            int cc=WEBSOCKET_DEFAULT_CLOSE_CODE; char reason[128]={0};
            if(plen>=2){
                uint16_t nc; memcpy(&nc,payload,2); cc=(int)ntohs(nc);
                if(plen>2){size_t rl=(size_t)plen-2;if(rl>=sizeof(reason))rl=sizeof(reason)-1;memcpy(reason,payload+2,rl);}
            } else if(ws->close_requested){
                cc=ws->requested_close_code;
                snprintf(reason,sizeof(reason),"%s",ws->requested_close_reason);
            }
            free(payload);
            pthread_mutex_lock(&ws->mutex);
            ws_send_frame_locked(ws,NULL,0,WS_OP_CLOSE); /* echo close */
            ws_enqueue_close_locked(ws,cc,reason);
            pthread_mutex_unlock(&ws->mutex);
            log_debug("WEBSOCKET: closed by peer %s code=%d reason=%s",ws->url,cc,reason);
            break;
        }

        /* data frames (text / binary / continuation) */
        if(op==WS_OP_TEXT||op==WS_OP_BINARY||op==WS_OP_CONTINUATION){
            if(op!=WS_OP_CONTINUATION){frag_text=(op==WS_OP_TEXT);frag_len=0;}
            size_t new_len=frag_len+(size_t)plen;
            if(new_len>frag_cap){
                size_t nc=new_len+4096;
                uint8_t *tmp=(uint8_t *)realloc(frag,nc);
                if(!tmp){free(payload);goto recv_error;}
                frag=tmp;frag_cap=nc;
            }
            if(payload&&plen>0)memcpy(frag+frag_len,payload,(size_t)plen);
            frag_len=new_len;free(payload);payload=NULL;
            if(fin){
                pthread_mutex_lock(&ws->mutex);
                ws_enqueue_locked(ws,WEBSOCKET_EVENT_MESSAGE,0,NULL,
                                   (const char *)frag,(int)frag_len,frag_text);
                pthread_mutex_unlock(&ws->mutex);
                frag_len=0;
            }
            continue;
        }

        free(payload);
        continue;

recv_error:
        pthread_mutex_lock(&ws->mutex);
        ws_enqueue_error_locked(ws);
        ws_enqueue_close_locked(ws,1006,"receive failed");
        pthread_mutex_unlock(&ws->mutex);
        break;
    }

    free(frag);
    pthread_mutex_lock(&ws->mutex);
    ws->connected=false;
    if(!ws->close_emitted)ws_enqueue_close_locked(ws,1006,"connection closed");
    ws->sockfd=WS_INVALID_SOCK;
    pthread_mutex_unlock(&ws->mutex);
    ws_sock_close(fd);
    return NULL;
}

/* ---------- Public API -------------------------------------------------- */

static pthread_once_t ws_net_once=PTHREAD_ONCE_INIT;
static void ws_net_init_once(void){ws_init_sockets();}

websocket_t *websocket_create(const char *url,
                               const websocket_callbacks_t *callbacks,
                               void *user_data)
{
    websocket_t *ws;
    if(!url)return NULL;
    pthread_once(&ws_net_once,ws_net_init_once);
    ws=(websocket_t *)calloc(1,sizeof(*ws));
    if(!ws)return NULL;
    if(pthread_mutex_init(&ws->mutex,NULL)!=0){free(ws);return NULL;}
    ws->sockfd=WS_INVALID_SOCK;
    snprintf(ws->url,sizeof(ws->url),"%s",url);
    ws->user_data=user_data;
    ws->requested_close_code=WEBSOCKET_DEFAULT_CLOSE_CODE;
    snprintf(ws->requested_close_reason,sizeof(ws->requested_close_reason),"client closing");
    if(callbacks)ws->callbacks=*callbacks;
    if(pthread_create(&ws->worker,NULL,websocket_worker_main,ws)!=0){
        pthread_mutex_destroy(&ws->mutex);free(ws);return NULL;
    }
    ws->worker_started=true;
    log_debug("WEBSOCKET: Connecting to %s...",ws->url);
    return ws;
}

void websocket_poll(websocket_t *ws)
{
    if(!ws)return;
    for(;;){
        websocket_event_t *ev;
        pthread_mutex_lock(&ws->mutex);
        ev=ws->event_head;
        if(ev){ws->event_head=ev->next;if(!ws->event_head)ws->event_tail=NULL;}
        pthread_mutex_unlock(&ws->mutex);
        if(!ev)break;
        switch(ev->type){
        case WEBSOCKET_EVENT_OPEN:
            if(ws->callbacks.on_open)ws->callbacks.on_open(ws,ws->user_data);break;
        case WEBSOCKET_EVENT_CLOSE:
            if(ws->callbacks.on_close)ws->callbacks.on_close(ws,ev->code,ev->reason,ws->user_data);break;
        case WEBSOCKET_EVENT_ERROR:
            if(ws->callbacks.on_error)ws->callbacks.on_error(ws,ws->user_data);break;
        case WEBSOCKET_EVENT_MESSAGE:
            if(ws->callbacks.on_message)ws->callbacks.on_message(ws,ev->message,ev->len,ev->is_text,ws->user_data);break;
        default:break;
        }
        free(ev->message);free(ev);
    }
}

void websocket_destroy(websocket_t *ws)
{
    websocket_event_t *ev;
    if(!ws)return;
    websocket_close(ws,WEBSOCKET_DEFAULT_CLOSE_CODE,"Client closing");
    pthread_mutex_lock(&ws->mutex);
    ws->destroy_requested=true;
    if(ws->sockfd!=WS_INVALID_SOCK)shutdown((int)ws->sockfd,WS_SHUT_RDWR);
    pthread_mutex_unlock(&ws->mutex);
    if(ws->worker_started)pthread_join(ws->worker,NULL);
    pthread_mutex_lock(&ws->mutex);
    ev=ws->event_head;ws->event_head=ws->event_tail=NULL;
    pthread_mutex_unlock(&ws->mutex);
    while(ev){websocket_event_t *nx=ev->next;free(ev->message);free(ev);ev=nx;}
    pthread_mutex_destroy(&ws->mutex);
    free(ws);
}

int websocket_send_text(websocket_t *ws, const char *text)
{
    int rc; if(!ws||!text)return -1;
    pthread_mutex_lock(&ws->mutex);
    rc=ws_send_frame_locked(ws,text,strlen(text),WS_OP_TEXT);
    pthread_mutex_unlock(&ws->mutex);
    return rc;
}

int websocket_send_binary(websocket_t *ws, const void *data, size_t len)
{
    int rc; if(!ws||!data||len==0)return -1;
    pthread_mutex_lock(&ws->mutex);
    rc=ws_send_frame_locked(ws,data,len,WS_OP_BINARY);
    pthread_mutex_unlock(&ws->mutex);
    return rc;
}

void websocket_close(websocket_t *ws, int code, const char *reason)
{
    if(!ws)return;
    pthread_mutex_lock(&ws->mutex);
    ws->close_requested=true;
    ws->requested_close_code=code>0?code:WEBSOCKET_DEFAULT_CLOSE_CODE;
    snprintf(ws->requested_close_reason,sizeof(ws->requested_close_reason),
             "%s",reason?reason:"");
    if(ws->connected)
        ws_send_close_frame_locked(ws,ws->requested_close_code,ws->requested_close_reason);
    pthread_mutex_unlock(&ws->mutex);
}

bool websocket_is_connected(const websocket_t *ws){return ws?ws->connected:false;}

const char *websocket_get_url(const websocket_t *ws){return ws?ws->url:NULL;}

#endif /* EMSCRIPTEN */
