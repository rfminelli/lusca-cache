/*
 * $Id$
 *
 * SASL authenticator module for Squid.
 * Copyright (C) 2002 Ian Castle <ian.castle@coldcomfortfarm.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111, USA.
 *
 * Install instructions:
 *
 * This program authenticates users against using cyrus-sasl
 *
 * Compile this program with: gcc -Wall -o sasl_auth sasl_auth.c -lsasl
 *
 */
#include <sasl.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#define APP_NAME_SASL	"squid_sasl_auth"

int
main()
{
	char line[8192];
	char *username, *password;
	const char *errstr;

	int rc;
        sasl_conn_t *conn = NULL;

	rc = sasl_server_init( NULL, APP_NAME_SASL );

	if ( rc != SASL_OK ) {
		fprintf( stderr, "error %d %s\n", rc, sasl_errstring(rc, NULL, NULL ));
		fprintf( stdout, "ERR\n" );
		return 1;
	}

	rc = sasl_server_new( APP_NAME_SASL, NULL, NULL, NULL, 0, &conn );

	if ( rc != SASL_OK ) {
		fprintf( stderr, "error %d %s\n", rc, sasl_errstring(rc, NULL, NULL ));
		fprintf( stdout, "ERR\n" );
		return 1;
	}

	while ( fgets( line, sizeof( line ), stdin )) {
		username = &line[0];
		password = strchr( line, '\n' );
		if ( !password) {
			fprintf( stderr, "authenticator: Unexpected input '%s'\n", line );
			fprintf( stdout, "ERR\n" );
			continue;
		}
		*password = '\0';
		password = strchr ( line, ' ' );
		if ( !password) {
			fprintf( stderr, "authenticator: Unexpected input '%s'\n", line );
			fprintf( stdout, "ERR\n" );
			continue;
		}
		*password++ = '\0';

		rc = sasl_checkpass(conn, username, strlen(username), password, strlen(password), &errstr);

		if ( rc != SASL_OK ) {
			if ( errstr ) {
				fprintf( stderr, "errstr %s\n", errstr );
			}
			if ( rc != SASL_BADAUTH ) {
				fprintf( stderr, "error %d %s\n", rc, sasl_errstring(rc, NULL, NULL ));
			}
			fprintf( stdout, "ERR\n" );
		}
		else {
			fprintf( stdout, "OK\n" );
		}

	}

        sasl_dispose( &conn );
        sasl_done();

	return 0;
}
