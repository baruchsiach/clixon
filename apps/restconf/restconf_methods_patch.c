/*
 *
  ***** BEGIN LICENSE BLOCK *****
 
  Copyright (C) 2009-2019 Olof Hagsand

  This file is part of CLIXON.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

  Alternatively, the contents of this file may be used under the terms of
  the GNU General Public License Version 3 or later (the "GPL"),
  in which case the provisions of the GPL are applicable instead
  of those above. If you wish to allow use of your version of this file only
  under the terms of the GPL, and not to allow others to
  use your version of this file under the terms of Apache License version 2, 
  indicate your decision by deleting the provisions above and replace them with
  the  notice and other provisions required by the GPL. If you do not delete
  the provisions above, a recipient may use your version of this file under
  the terms of any one of the Apache License version 2 or the GPL.

  ***** END LICENSE BLOCK *****
  
 */

/*
 * See rfc8040

 * sudo apt-get install libfcgi-dev
 * gcc -o fastcgi fastcgi.c -lfcgi

 * sudo su -c "/www-data/clixon_restconf -D 1 f /usr/local/etc/example.xml " -s /bin/sh www-data

 * This is the interface:
 * api/data/profile=<name>/metric=<name> PUT data:enable=<flag>
 * api/test
   +----------------------------+--------------------------------------+
   | 100 Continue               | POST accepted, 201 should follow     |
   | 200 OK                     | Success with response message-body   |
   | 201 Created                | POST to create a resource success    |
   | 204 No Content             | Success without response message-    |
   |                            | body                                 |
   | 304 Not Modified           | Conditional operation not done       |
   | 400 Bad Request            | Invalid request message              |
   | 401 Unauthorized           | Client cannot be authenticated       |
   | 403 Forbidden              | Access to resource denied            |
   | 404 Not Found              | Resource target or resource node not |
   |                            | found                                |
   | 405 Method Not Allowed     | Method not allowed for target        |
   |                            | resource                             |
   | 409 Conflict               | Resource or lock in use              |
   | 412 Precondition Failed    | Conditional method is false          |
   | 413 Request Entity Too     | too-big error                        |
   | Large                      |                                      |
   | 414 Request-URI Too Large  | too-big error                        |
   | 415 Unsupported Media Type | non RESTCONF media type              |
   | 500 Internal Server Error  | operation-failed                     |
   | 501 Not Implemented        | unknown-operation                    |
   | 503 Service Unavailable    | Recoverable server error             |
   +----------------------------+--------------------------------------+
Mapping netconf error-tag -> status code
                 +-------------------------+-------------+
                 | <error&#8209;tag>       | status code |
                 +-------------------------+-------------+
                 | in-use                  | 409         |
                 | invalid-value           | 400         |
                 | too-big                 | 413         |
                 | missing-attribute       | 400         |
                 | bad-attribute           | 400         |
                 | unknown-attribute       | 400         |
                 | bad-element             | 400         |
                 | unknown-element         | 400         |
                 | unknown-namespace       | 400         |
                 | access-denied           | 403         |
                 | lock-denied             | 409         |
                 | resource-denied         | 409         |
                 | rollback-failed         | 500         |
                 | data-exists             | 409         |
                 | data-missing            | 409         |
                 | operation-not-supported | 501         |
                 | operation-failed        | 500         |
                 | partial-operation       | 500         |
                 | malformed-message       | 400         |
                 +-------------------------+-------------+

 * "api-path" is "URI-encoded path expression" definition in RFC8040 3.5.3
 */

#ifdef HAVE_CONFIG_H
#include "clixon_config.h" /* generated by config & autoconf */
#endif
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <syslog.h>
#include <fcntl.h>
#include <assert.h>
#include <time.h>
#include <signal.h>
#include <limits.h>
#include <sys/time.h>
#include <sys/wait.h>

/* cligen */
#include <cligen/cligen.h>

/* clicon */
#include <clixon/clixon.h>

#include <fcgiapp.h> /* Need to be after clixon_xml-h due to attribute format */

#include "restconf_lib.h"
#include "restconf_methods_patch.h"


/*! Generic REST PATCH  method 
 * @param[in]  h      CLIXON handle
 * @param[in]  r      Fastcgi request handle
 * @param[in]  api_path According to restconf (Sec 3.5.3.1 in rfc8040)
 * @param[in]  pcvec  Vector of path ie DOCUMENT_URI element
 * @param[in]  pi     Offset, where to start pcvec
 * @param[in]  qvec   Vector of query string (QUERY_STRING)
 * @param[in]  data   Stream input data
 * Netconf:  <edit-config> (nc:operation="merge")      
 * See RFC8040 Sec 4.6
 */
int
api_data_patch(clicon_handle h,
	       FCGX_Request *r, 
	       char         *api_path, 
	       cvec         *pcvec, 
	       int           pi,
	       cvec         *qvec, 
	       char         *data)
{
    notimplemented(r);
    return 0;
}
