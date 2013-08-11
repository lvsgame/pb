local lpeg = require "lpeg"

local tinsert = table.insert
local sfind = string.find
local ssub = string.sub
local sfmt = string.format
local supper = string.upper
local smatch = string.match
local srep = string.rep
local P = lpeg.P
local S = lpeg.S
local R = lpeg.R
local C = lpeg.C
local Cc = lpeg.Cc
local Cs = lpeg.Cs
local Cg = lpeg.Cg
local Ct = lpeg.Ct
local Cmt = lpeg.Cmt
local Carg = lpeg.Carg

local function _MC(pattern)
    return Cmt(C(pattern) * Carg(1), 
        function (_, pos, pattern, context)
            if pos > context.pos then 
                context.pos = pos 
            end 
            return true
        end)
end
local function _M(pattern)
    return Cmt(pattern * Carg(1), 
        function (_, pos, context)
            if pos > context.pos then 
                context.pos = pos 
            end 
            return true
        end)
end
local _MLine = Cmt((P("\n") + "\r\n") * Carg(1), 
        function (_, pos, context)
            if pos > context.pos then
                context.pos = pos
                context.line = context.line + 1
                context.line_pos = pos
            end
            return true
        end)

local _MError = Cmt(Carg(1), function (buffer, pos, context)
        local err_line = ssub(buffer, context.line_pos, 
                sfind(buffer, "\n", context.line_pos))
        local pos = context.pos - context.line_pos
        err_line = sfmt("%s`%s", ssub(err_line, 1, pos), ssub(err_line, pos+1))
        error(sfmt("syntax error [%s:%d:%d]%s", 
            context.file, context.line, pos+1, err_line))
        end)

local blank = S(" \t") + _MLine
local blanks = blank^1
local empty = blank^0
local digit = R("09")
local alpha = R("az", "AZ")
local varname = (alpha + "_") * (alpha + "_" + digit)^0

local scomment = P("//") * (1 - _MLine)^0 * _MLine
local mcomment = P("/*") * (1 - (P("/*")+"*/"))^0 * P("*/")
local comment = scomment + mcomment + blanks

local label = P("required") + "optional" + "repeated"
local ptype = varname
local number = digit^1

local expression = (empty * _M(S("+-"))^-1 * 
    empty * (digit^1 + varname))^1

local repeat_expression = _M("[") * empty *
    (_M("max") * empty * _M("=") * empty * 
     Cg(Cc(true), "repeatc_isvar") + 
     Cg(Cc(false), "repeatc_isvar")) *
    Cg(_MC(expression), "repeatc") * empty * _M("]")

local comment = Ct(
    Cg(Cc("comment"), "sign") *
    Cg(_MC(scomment + mcomment + blanks), "value"))

local message_field = Ct(
    Cg(Cc("message_field"), "sign") *
    Cg(_MC(label), "label") * blanks * 
    Cg(_MC(varname), "type") * blanks * 
    Cg(_MC(varname), "name") * empty * 
    repeat_expression^0 * empty * _M("=") * empty * 
    Cg(_MC(number), "number") * empty * _M(";"))

local enum_field = Ct(
    Cg(Cc("enum_field"), "sign") *
    Cg(_MC(varname), "name") * empty * 
   (_M("=") * empty *
    Cg(_MC((digit^1 + varname)), "value") * empty)^-1 * _M(S(",;")))

local message = Ct(
    Cg(_MC("message"), "sign") * blanks * 
    Cg(_MC(varname), "name") * empty * _M("{") *
    Cg(Ct((message_field + comment)^0), "fields") * empty * 
    _M("}") * _M(S(";"))^0)

local enum = Ct(
    Cg(_MC("enum"), "sign") * blanks *
    Cg(_MC(varname), "name") * empty * _M("{") *
    Cg(Ct((enum_field + comment)^0), "fields") * empty *
    _M("}") * _M(S(";"))^0)

local import = Ct(
    Cg(_MC("import"), "sign") * blanks *
    Cg(_MC(varname), "value"))

local pragma = Ct(
    Cg(Cc("pragma"), "sign") *
    Cg(_M("#pragma") * blanks * _M("pack") * empty * 
       _M("(") * empty * _M(digit^-1) * empty * _M(")"), "value"))

local pattern = Ct((import + message + enum + comment + pragma)^0)

-------------------------------------------------------------------------------
-- serialize
-------------------------------------------------------------------------------
_TYPES = {
    ["bool"]    = "bool",
    ["uint8"]   = "uint8_t",
    ["uint16"]  = "uint16_t",
    ["uint32"]  = "uint32_t",
    ["uint64"]  = "uint64_t",
    ["int8"]    = "int8_t",
    ["int16"]   = "int16_t",
    ["int32"]   = "int32_t",
    ["int64"]   = "int64_t",
    ["fixed32"] = "uint32_t",
    ["fixed64"] = "uint64_t",
    ["sfixed32"]= "int32_t",
    ["sfixed64"]= "int64_t",
    ["float"]   = "float",
    ["double"]  = "double",
    ["string"]  = "char",
    ["bytes"]   = "uint8_t",
}

_LABELS = {
    ["required"] = "Y",
    ["optional"] = "O",
    ["repeated"] = "R",
}

local function _dump_instruction(t)
    tinsert(t, '/*this file is generate by protoc.lua do not change it by hand*/\n')
end

local function _dump_h_header(outname, t)
    tinsert(t, sfmt("#ifndef __%s_H__\n", outname))
    tinsert(t, sfmt("#define __%s_H__\n", outname))
end

local function _dump_h_tail(t)
    tinsert(t, "#endif")
end

local function _dump_field(f, t) 
    if f.sign == "message_field" then
        if f.repeatc_isvar then
            tinsert(t, sfmt("uint16_t n%s;\n    ", f.name))
        end
        local repeatc = f.repeatc and sfmt("[%s]", f.repeatc) or ""
        local ftype = _TYPES[f.type] or ("struct " .. f.type)
        tinsert(t, sfmt("%s %s%s;", ftype, f.name, repeatc))
    elseif f.sign == "enum_field" then
        local value = f.value and " = " .. f.value or ""
        tinsert(t, f.name .. value .. ",")
    else
        tinsert(t, f.value)
    end
end

local function _dump_proto(proto, t) 
    _dump_instruction(t)
    _dump_h_header(sfmt(supper(proto.name).."_PB"), t)
    tinsert(t, '#include <stdint.h>\n')
    for _, b in ipairs(proto) do
        if b.sign == "import" then
            tinsert(t, sfmt('#include "%s.pb.h"', b.value))
        elseif b.sign == "message" then
            tinsert(t, sfmt("struct %s {", b.name))
            for _, f in ipairs(b.fields) do
                _dump_field(f, t)
            end
            tinsert(t, "};")
        elseif b.sign == "enum" then
            tinsert(t, sfmt("enum %s {", b.name))
            for _, f in ipairs(b.fields) do
                _dump_field(f, t)
            end
            tinsert(t, "};")
        else
            tinsert(t, b.value)
        end
    end
    _dump_h_tail(t)
end

local function _ismessage(typename, messages)
    for _, m in ipairs(messages) do
        if m.name == typename then
            return true
        end
    end
end

local function _verify_label(f, m)
    assert(_LABELS[f.label], 
        sfmt("%s::%s unknown label#%s", m.name, f.name, f.label))
    assert((not f.repeatc_isvar) or (f.repeatc_isvar and f.label == "repeated"), 
        sfmt("%s::%s label must be repeated", m.name, f.name))
end

local function _verify_type(f, m, messages)
    local typename = _TYPES[f.type]
    if not typename then
        if _ismessage(f.type, messages) then
            f.usertype = true
        else
            error(sfmt("%s::%s unknown type#%s", m.name, f.name, f.type))
        end
    end
end

local function _verify_number(f, m)
    local number = smatch(f.number, "%d+") + 0
    assert(number >= 1 and number <= 4096, 
        sfmt("%s::%s number must be 1~4096", m.name, f.name))
end

local function _verify(messages)
    for _, m in ipairs(messages) do
        for _, f in ipairs(m.fields) do
            if f.sign == "message_field" then
                _verify_label(f, m)
                _verify_type(f, m, messages)
                _verify_number(f, m)
            end
        end
    end
end

local function _dump_message_field_decls(m, t)
    for _, f in ipairs(m.fields) do
        if f.sign == "message_field" then
            tinsert(t, sfmt(
                '        {"%s", %s, offsetof(struct %s, %s), %s, %s, %s, "%s%s"},\n', 
                f.name, 
                f.number, 
                m.name, 
                f.name, 
                f.usertype and sfmt("sizeof(struct %s)", f.type) or "0",
                f.repeatc and f.repeatc or "0",
                f.repeatc_isvar and sfmt(
                    "\n         offsetof(struct %s, %s) - offsetof(struct %s, n%s)",
                                        m.name, 
                                        f.name, 
                                        m.name, 
                                        f.name) or "0",
                _LABELS[f.label], 
                f.type))
        end
    end
end

local function _pbo_name(name)
    return sfmt("PBO_%s", supper(name))
end

local function _dump_message(m, i, t)
    tinsert(t, sfmt(
             "    struct pb_field_decl fds%d[] = {\n", i))
    _dump_message_field_decls(m, t)
    tinsert(t, "    };\n")
    tinsert(t, sfmt(
             '    %s = pb_context_object(pbc, "%s", fds%d, sizeof(fds%d)/sizeof(fds%d[0]));\n', 
                                              _pbo_name(m.name), m.name, i, i, i))
    tinsert(t, sfmt(
            "    if (%s == NULL) {\n", _pbo_name(m.name)))
    tinsert(t, '        PB_LOG("pb object error: %s", pb_context_lasterror(pbc));\n')
    tinsert(t, "        pb_context_delete(pbc);\n")
    tinsert(t, "        return NULL;\n")
    tinsert(t, "    }\n\n")
end

local function _dump_messages(messages, t)
    local i = 1 
    for _, m in ipairs(messages) do
        _dump_message(m, i, t)
        i = i+1
    end
end

local function _get_messages(protos)
    local messages = {}
    for _, proto in ipairs(protos) do
        for _, b in ipairs(proto) do
            if b.sign == "message" then
                messages[#messages+1] = b
            end
        end
    end
    return messages;
end

local function _dump_includes(protos, t)
    tinsert(t, '#include <stddef.h>\n')
    tinsert(t, '#include "pb.h"\n')
    tinsert(t, '#include "pb_log.h"\n')
    for _, proto in ipairs(protos) do
        tinsert(t, sfmt('#include "%s.pb.h"\n', proto.name))
    end
end

local function _dump_pbobject_decl(messages, t)
    for _, m in ipairs(messages) do
        tinsert(t, sfmt("struct pb_object* %s = NULL;\n", _pbo_name(m.name)))
    end
end

local function _dump_context(protos, outname, t)
    local messages = _get_messages(protos)
    _verify(messages)
    _dump_instruction(t)
    _dump_h_header(supper(outname), t)
    _dump_includes(protos, t) 
    _dump_pbobject_decl(messages, t) 
    tinsert(t, "struct pb_context*\n")
    tinsert(t, "PB_CONTEXT_INIT() {\n")
    tinsert(t, sfmt(
             "    struct pb_context* pbc = pb_context_new(%d);\n", #messages))
    tinsert(t, "    if (pbc == NULL) {\n")
    tinsert(t, "        return NULL;\n")
    tinsert(t, "    }\n")
    _dump_messages(messages, t)
    tinsert(t, "    pb_context_fresh(pbc);\n");
    tinsert(t, "    if (pb_context_verify(pbc)) {\n");
    tinsert(t, '        PB_LOG("pb verify error: %s", pb_context_lasterror(pbc));\n')
    tinsert(t, "        return NULL;\n")
    tinsert(t, "    }\n")
    tinsert(t, "    return pbc;\n");
    tinsert(t, "}\n\n");
    _dump_h_tail(t);
end

-------------------------------------------------------------------------------
-- parser
-------------------------------------------------------------------------------
local plp = {}

local function _parse(s, protoname, filename)
    local context = { file=filename, pos=0, line=1, line_pos=0 }
    local proto = lpeg.match(pattern * (-1 + _MError), s, 1, context)
    proto.name = protoname
    return proto
end

function plp.parse_string(s, protoname)
    return _parse(s, protoname, "")
end

function plp.parse_file(filename, protoname)
    print('>> parsing file ' .. filename)
	local fp = io.open(filename, "r")
	local s = fp:read("*a")
	fp:close()
	return _parse(s, protoname, filename)
end

function plp.dump_proto(proto, out)
    local old_output = io.output()
    if out then
        io.output(out)
    end
    local t = {}
    _dump_proto(proto, t)
    io.write(table.concat(t))
    io.output(old_output)
end

function plp.dump_context(protos, out, outname)
    local old_output = io.output()
    if out then
        io.output(out)
    end
    local t = {}
    _dump_context(protos, outname, t)
    io.write(table.concat(t))
    io.output(old_output)
end

return plp
