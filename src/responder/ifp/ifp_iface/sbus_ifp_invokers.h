/*
    Generated by sbus code generator

    Copyright (C) 2017 Red Hat

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _SBUS_IFP_INVOKERS_H_
#define _SBUS_IFP_INVOKERS_H_

#include <talloc.h>
#include <tevent.h>
#include <dbus/dbus.h>

#include "sbus/sbus_interface_declarations.h"
#include "sbus/sbus_request.h"

#define _sbus_ifp_declare_invoker(input, output)                               \
    struct tevent_req *                                                   \
    _sbus_ifp_invoke_in_ ## input ## _out_ ## output ## _send                 \
        (TALLOC_CTX *mem_ctx,                                             \
         struct tevent_context *ev,                                       \
         struct sbus_request *sbus_req,                                   \
         sbus_invoker_keygen keygen,                                      \
         const struct sbus_handler *handler,                              \
         DBusMessageIter *read_iterator,                                  \
         DBusMessageIter *write_iterator,                                 \
         const char **_key)

_sbus_ifp_declare_invoker(, );
_sbus_ifp_declare_invoker(, ao);
_sbus_ifp_declare_invoker(, as);
_sbus_ifp_declare_invoker(, b);
_sbus_ifp_declare_invoker(, ifp_extra);
_sbus_ifp_declare_invoker(, o);
_sbus_ifp_declare_invoker(, s);
_sbus_ifp_declare_invoker(, u);
_sbus_ifp_declare_invoker(s, ao);
_sbus_ifp_declare_invoker(s, as);
_sbus_ifp_declare_invoker(s, o);
_sbus_ifp_declare_invoker(s, s);
_sbus_ifp_declare_invoker(sas, raw);
_sbus_ifp_declare_invoker(ss, o);
_sbus_ifp_declare_invoker(ssu, ao);
_sbus_ifp_declare_invoker(su, ao);
_sbus_ifp_declare_invoker(u, o);

#endif /* _SBUS_IFP_INVOKERS_H_ */
