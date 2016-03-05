#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <signal.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>
#include <libwebsockets.h>
#include <syslog.h>
#include <sys/time.h>
#include <unistd.h>


volatile int force_exit = 0;
struct lws_context *context;

/* http server gets files from this path */
#define LOCAL_RESOURCE_PATH "/usr/local/share/web"
char *resource_path = LOCAL_RESOURCE_PATH;

/* singlethreaded version */


void dump_handshake_info(struct lws *wsi);

struct per_session_data__http {
	lws_filefd_type fd;
};

struct per_session_data__dumb_increment {
	int number;
};

int callback_http(struct lws *wsi, enum lws_callback_reasons reason, void *user,
	      void *in, size_t len);


int callback_dumb_increment(struct lws *wsi, enum lws_callback_reasons reason,
			void *user, void *in, size_t len);


/* list of supported protocols and callbacks */
enum supported_protocols {
	/* always first */
	PROTOCOL_HTTP = 0,

	PROTOCOL_DUMB_INCREMENT,
};

static struct lws_protocols protocols[] = {
	/* first protocol must always be HTTP handler */

	{
		"http-only",		/* name */
		callback_http,		/* callback */
		sizeof (struct per_session_data__http),	/* per_session_data_size */
		0,			/* max frame size / rx buffer */
	},
	{
		"dumb-increment-protocol",
		callback_dumb_increment,
		sizeof(struct per_session_data__dumb_increment),
		10,
	},
	{ NULL, NULL, 0, 0 } /* terminator */
};

void sighandler(int sig)
{
	force_exit = 1;
	lws_cancel_service(context);
}

float threshold=20.5;

int callback_dumb_increment(struct lws *wsi, enum lws_callback_reasons reason,
			void *user, void *in, size_t len)
{
	unsigned char buf[LWS_SEND_BUFFER_PRE_PADDING + 512 +
						  LWS_SEND_BUFFER_POST_PADDING];
	struct per_session_data__dumb_increment *pss =
			(struct per_session_data__dumb_increment *)user;
	unsigned char *p = &buf[LWS_SEND_BUFFER_PRE_PADDING];
	int n, m;
	int fd;
    	char *path = "/sys/bus/w1/devices/28-0000034a8e9d/w1_slave";
	char buffer[1000];
	int length;
	char temp[10];
	float temperature;
	

	switch (reason) {

	case LWS_CALLBACK_ESTABLISHED:
		break;

	case LWS_CALLBACK_SERVER_WRITEABLE:


		if ((fd = open(path, O_RDONLY)) < 0) 
		{
			perror("Erreur ouverture de value de w1_slave");
			exit(EXIT_FAILURE);
		}

		length=read(fd, buffer, sizeof(buffer));
		
		strncpy(temp, buffer+length-6, 5);	
		
		temperature=atof(temp)/1000;

		close(fd);
	   			
		n = sprintf((char *)p, "%2.3f;%2.1f", temperature, threshold);
		m = lws_write(wsi, p, n, LWS_WRITE_TEXT);
		if (m < n) {
			lwsl_err("ERROR %d writing to di socket\n", n);
			return -1;
		}

		break;

	case LWS_CALLBACK_RECEIVE:
		
		threshold = atof((char *)in);
		
		break;

	default:
		break;
	}

	return 0;
}

int main(int argc, char **argv)
{
	struct lws_context_creation_info info;
	char interface_name[128] = "";
	unsigned int ms, oldms = 0;
	const char *iface = NULL;
 	int debug_level = 7;
	int opts = 0;
	int n = 0;

	/*
	 * take care to zero down the info struct, he contains random garbaage
	 * from the stack otherwise
	 */
	memset(&info, 0, sizeof info);
	info.port = 9000;

	signal(SIGINT, sighandler);


	/* tell the library what debug level to emit and to send it to syslog */
	lws_set_log_level(debug_level, lwsl_emit_syslog);

	printf("\n\nServeur websocket MS 2015 \n"
		"------------------------ \n"
		"Utilise libwebsockets \n"
		"(C) Copyright 2010-2015 Andy Green <andy@warmcat.com> \n "
		"licensed under LGPL2.1\n\nCtr+C pour stoper\n\n");

	printf("Using resource path \"%s\"\nWait for connexions\n\n", resource_path);


	info.iface = iface;
	info.protocols = protocols;

	info.extensions = lws_get_internal_extensions();


	info.ssl_cert_filepath = NULL;
	info.ssl_private_key_filepath = NULL;

	info.gid = -1;
	info.uid = -1;
	info.options = opts;

	context = lws_create_context(&info);
	if (context == NULL) {
		lwsl_err("libwebsocket init failed\n");
		return -1;
	}



	n = 0;
	while (n >= 0 && !force_exit) {
		struct timeval tv;

		gettimeofday(&tv, NULL);

		/*
		 * This provokes the LWS_CALLBACK_SERVER_WRITEABLE for every
		 * live websocket connection using the DUMB_INCREMENT protocol,
		 * as soon as it can take more packets (usually immediately)
		 */

		ms = (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
		if ((ms - oldms) > 50) {
			lws_callback_on_writable_all_protocol(context,
				&protocols[PROTOCOL_DUMB_INCREMENT]);
			oldms = ms;
		}


		/*
		 * If libwebsockets sockets are all we care about,
		 * you can use this api which takes care of the poll()
		 * and looping through finding who needed service.
		 *
		 * If no socket needs service, it'll return anyway after
		 * the number of ms in the second argument.
		 */

		n = lws_service(context, 250);
	}

	lws_context_destroy(context);
	printf("\n\nwsServer exited cleanly\n");


	return 0;
}


void dump_handshake_info(struct lws *wsi)
{
	int n = 0;
	char buf[256];
	const unsigned char *c;

	do {
		c = lws_token_to_string(n);
		if (!c) {
			n++;
			continue;
		}

		if (!lws_hdr_total_length(wsi, n)) {
			n++;
			continue;
		}

		lws_hdr_copy(wsi, buf, sizeof buf, n);

		fprintf(stderr, "    %s = %s\n", (char *)c, buf);
		n++;
	} while (c);
}

const char * get_mimetype(const char *file)
{
	int n = strlen(file);

	if (n < 5)
		return NULL;

	if (!strcmp(&file[n - 4], ".ico"))
		return "image/x-icon";

	if (!strcmp(&file[n - 4], ".png"))
		return "image/png";

	if (!strcmp(&file[n - 5], ".html"))
		return "text/html";

	return NULL;
}

int callback_http(struct lws *wsi, enum lws_callback_reasons reason, void *user,
		  void *in, size_t len)
{
	struct per_session_data__http *pss =
			(struct per_session_data__http *)user;

	const char *mimetype;
	char *other_headers;
	char buf[256];
	int n, m;


	switch (reason) {
	case LWS_CALLBACK_HTTP:

		dump_handshake_info(wsi);

		/* dump the individual URI Arg parameters */
		n = 0;
		while (lws_hdr_copy_fragment(wsi, buf, sizeof(buf),
					     WSI_TOKEN_HTTP_URI_ARGS, n) > 0) {
			lwsl_info("URI Arg %d: %s\n", ++n, buf);
		}

		if (len < 1) {
			lws_return_http_status(wsi,
						HTTP_STATUS_BAD_REQUEST, NULL);
			goto try_to_reuse;
		}

		/* this example server has no concept of directories */
		if (strchr((const char *)in + 1, '/')) {
			lws_return_http_status(wsi,
					       HTTP_STATUS_FORBIDDEN, NULL);
			goto try_to_reuse;
		}

		/* if a legal POST URL, let it continue and accept data */
		if (lws_hdr_total_length(wsi, WSI_TOKEN_POST_URI))
			return 0;

		
		/* if not, send a file the easy way */
		strcpy(buf, resource_path);
		if (strcmp(in, "/")) {
			if (*((const char *)in) != '/')
				strcat(buf, "/");
			strncat(buf, in, sizeof(buf) - strlen(resource_path));
		} else /* default file to serve */
			strcat(buf, "/index.html");
		buf[sizeof(buf) - 1] = '\0';

		/* refuse to serve files we don't understand */
		mimetype = get_mimetype(buf);
		if (!mimetype) {
			lwsl_err("Unknown mimetype for %s\n", buf);
			lws_return_http_status(wsi,
				      HTTP_STATUS_UNSUPPORTED_MEDIA_TYPE, NULL);
			return -1;
		}

		

		n = lws_serve_http_file(wsi, buf, mimetype, other_headers, n);
		if (n < 0 || ((n > 0) && lws_http_transaction_completed(wsi)))
			return -1; /* error or can't reuse connection: close the socket */

		/*
		 * notice that the sending of the file completes asynchronously,
		 * we'll get a LWS_CALLBACK_HTTP_FILE_COMPLETION callback when
		 * it's done
		 */

		break;

	case LWS_CALLBACK_HTTP_BODY:
		strncpy(buf, in, 20);
		buf[20] = '\0';
		if (len < 20)
			buf[len] = '\0';

		printf("LWS_CALLBACK_HTTP_BODY: %s... len %d\n",
				(const char *)buf, (int)len);

		break;

	default:
		break;
	}

	return 0;

	/* if we're on HTTP1.1 or 2.0, will keep the idle connection alive */
try_to_reuse:
	if (lws_http_transaction_completed(wsi))
		return -1;

	return 0;
}

