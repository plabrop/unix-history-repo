/*
 * Copyright (c) 1997 - 2003 Kungliga Tekniska H�gskolan
 * (Royal Institute of Technology, Stockholm, Sweden). 
 * All rights reserved. 
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met: 
 *
 * 1. Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright 
 *    notice, this list of conditions and the following disclaimer in the 
 *    documentation and/or other materials provided with the distribution. 
 *
 * 3. Neither the name of the Institute nor the names of its contributors 
 *    may be used to endorse or promote products derived from this software 
 *    without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND 
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE 
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS 
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY 
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE. 
 */

#include "ntlm/ntlm.h"

RCSID("$Id: delete_sec_context.c 22163 2007-12-04 21:25:06Z lha $");

OM_uint32 _gss_ntlm_delete_sec_context
           (OM_uint32 * minor_status,
            gss_ctx_id_t * context_handle,
            gss_buffer_t output_token
           )
{
    if (context_handle) {
	ntlm_ctx ctx = (ntlm_ctx)*context_handle;
	gss_cred_id_t cred = (gss_cred_id_t)ctx->client;

	*context_handle = GSS_C_NO_CONTEXT;

	if (ctx->server)
	    (*ctx->server->nsi_destroy)(minor_status, ctx->ictx);

	_gss_ntlm_release_cred(NULL, &cred);

	memset(ctx, 0, sizeof(*ctx));
	free(ctx);
    }
    if (output_token) {
	output_token->length = 0;
	output_token->value  = NULL;
    }

    *minor_status = 0;
    return GSS_S_COMPLETE;
}
