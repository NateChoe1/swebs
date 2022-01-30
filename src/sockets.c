/*
   swebs - a simple web server
   Copyright (C) 2022  Nate Choe
   
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <assert.h>
#include <stdlib.h>

#include <fcntl.h>
#include <unistd.h>
#include <netinet/in.h>
#include <gnutls/gnutls.h>

#include <sockets.h>

int initTLS() {
	assert(gnutls_global_init() >= 0);
	return 0;
}

Listener *createListener(SocketType type, uint16_t port, int backlog, ...) {
	Listener *ret = malloc(sizeof(Listener));
	if (ret == NULL)
		return NULL;
	ret->type = type;
	ret->fd = socket(AF_INET, SOCK_STREAM, 0);
	if (ret->fd < 0) {
		free(ret);
		return NULL;
	}
	int opt = 1;
	if (setsockopt(ret->fd, SOL_SOCKET,
	               SO_REUSEPORT,
	               &opt, sizeof(opt)) < 0) {
		goto error;
	}
	ret->addr.sin_family = AF_INET;
	ret->addr.sin_addr.s_addr = INADDR_ANY;
	ret->addr.sin_port = htons(port);
	ret->addrlen = sizeof(ret->addr);
	if (bind(ret->fd, (struct sockaddr *) &ret->addr, ret->addrlen) < 0)
		goto error;
	if (listen(ret->fd, backlog) < 0)
		goto error;

	va_list ap;
	va_start(ap, backlog);
	switch (type) {
		case TCP: default:
			break;
		case TLS: {
			char *keyfile = va_arg(ap, char *);
			char *certfile = va_arg(ap, char *);
			if (gnutls_certificate_allocate_credentials(&ret->creds)
			    < 0)
				goto error;
			if (gnutls_certificate_set_x509_key_file(ret->creds,
					certfile, keyfile,
					GNUTLS_X509_FMT_PEM) < 0)
				goto error;
			if (gnutls_priority_init(&ret->priority,
					NULL, NULL) < 0)
				goto error;
#if GNUTLS_VERSION_NUMBER >= 0x030506
			gnutls_certificate_set_known_dh_params(ret->creds,
					GNUTLS_SEC_PARAM_MEDIUM);
#endif
			break;
		}
	}
	va_end(ap);
	return ret;
error:
	close(ret->fd);
	free(ret);
	return NULL;
}

Stream *acceptStream(Listener *listener) {
	Stream *ret = malloc(sizeof(Stream));
	if (ret == NULL)
		return NULL;
	ret->type = listener->type;
	ret->fd = accept(listener->fd, (struct sockaddr *) &listener->addr,
	                               &listener->addrlen);

	if (ret->fd < 0) {
		free(ret);
		return NULL;
	}

	switch (listener->type) {
		case TCP: default:
			break;
		case TLS:
			if (gnutls_init(&ret->session, GNUTLS_SERVER) < 0)
				goto error;
			if (gnutls_priority_set(ret->session,
					listener->priority) < 0)
				goto error;
			if (gnutls_credentials_set(ret->session,
					GNUTLS_CRD_CERTIFICATE,
					listener->creds) < 0)
				goto error;
			gnutls_certificate_server_set_request(ret->session,
					GNUTLS_CERT_IGNORE);
			gnutls_handshake_set_timeout(ret->session,
					GNUTLS_DEFAULT_HANDSHAKE_TIMEOUT);
			gnutls_transport_set_int(ret->session, ret->fd);
			if (gnutls_handshake(ret->session) < 0)
				goto error;
			break;
	}
	return ret;
error:
	close(ret->fd);
	free(ret);
	return NULL;
}

void freeListener(Listener *listener) {
	if (listener->type == TLS) {
		gnutls_certificate_free_credentials(listener->creds);
		gnutls_priority_deinit(listener->priority);
	}
	close(listener->fd);
	free(listener);
}

void freeStream(Stream *stream) {
	if (stream->type == TLS) {
		gnutls_bye(stream->session, GNUTLS_SHUT_RDWR);
		gnutls_deinit(stream->session);
	}
	close(stream->fd);
	free(stream);
}

ssize_t sendStream(Stream *stream, void *data, size_t len) {
	switch (stream->type) {
		case TCP:
			return write(stream->fd, data, len);
		case TLS:
			return gnutls_record_send(stream->session, data, len);
		default:
			return -1;
	}
}

ssize_t recvStream(Stream *stream, void *data, size_t len) {
	switch (stream->type) {
		case TCP:
			return read(stream->fd, data, len);
		case TLS:
			return gnutls_record_recv(stream->session, data, len);
		default:
			return -1;
	}
}
