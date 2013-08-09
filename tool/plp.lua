-------------------------------------------------------------------------------------
-- plp
-------------------------------------------------------------------------------------

-- all part of field
local function getfield_label(value)
	local fields = block_cache[#block_cache].fields
	fields[#fields].label = value
end
local function getfield_type(value)
	local fields = block_cache[#block_cache].fields
	fields[#fields].type = value
end
local function getfield_name(value)
	local fields = block_cache[#block_cache].fields
	fields[#fields].name = value
end
local function getfield_order(value)
	local fields = block_cache[#block_cache].fields
	fields[#fields].order = value
end
local function getfield_value(value)
	local fields = block_cache[#block_cache].fields
	fields[#fields].value = value
end
local function getfield_fixrepeat(value)
	local fields = block_cache[#block_cache].fields
	fields[#fields].fixrepeat = value
end
local function getfield_varrepeat(value)
	local fields = block_cache[#block_cache].fields
	fields[#fields].varrepeat = value
end

local function _new_field()
    return { sign="", label="", type="", name="", value="", order="", fixrepeat="", varrepeat=""}
end

local function _new_block(sign, name)
    -- type: message enum comment import
    return { sign=sign, name=name, fields={}}
end

-- comment field
local function getfield_comment(value)
    --print('\tparse get comment : ' .. value)
    local fields = block_cache[#block_cache].fields
	fields[#fields].sign = "comment"
    fields[#fields].value = value
    fields[#fields+1] = _new_field() -- next field
end

-- field
local function getmfield(value)
    print('\tparse get message field : ' .. value)
	local fields = block_cache[#block_cache].fields
    fields[#fields].sign = "mfield"
	fields[#fields+1] = _new_field() -- next field
    fields[#fields].order = #fields
    
end
local function getefield(value)
    print('\tparse get enum field : ' .. value)
	local fields = block_cache[#block_cache].fields
    fields[#fields].sign = "efield"
	fields[#fields+1] = _new_field() -- next field
end

-- block
local function getblock(sign, name)
    print('parse begin ' .. sign .. ' -> ' .. name)
    local block = _new_block(sign, name)
    block.fields[1] = _new_field() -- for first field
    block.fields[1].order = 1
    block_cache[#block_cache+1] = block
end
local function endblock(sign)
    print('parse end ' .. sign)
    local fields = block_cache[#block_cache].fields
    fields[#fields] = nil
end

-- message
local function getmessage(name)
    getblock("message", name);
end
local function endmessage(value)
    endblock("message")
end

-- enum
local function getenum(name)
    getblock("enum", name);
end
local function endenum(value)
    endblock("enum")
end
local function getenum_unname(name)
    getblock("enum", "");
end
local function getenum_name(name)
    local fields = block_cache[#block_cache].fields
    fields[#fields].name=name 
end

-- import
local function getimport(name)
    print('parse get import : ' .. name)
    block_cache[#block_cache+1] = _new_block("import", name)
end

-- comment
local function getblock_comment(value)
    --print('parse get comment : ' .. value)
    block_cache[#block_cache+1] = _new_block("comment", value)
end

-- pragma pack
local function getblock_pack(value)
    block_cache[#block_cache+1] = _new_block("pragma pack", value)
end

-----------------------------------------------------------------------------------
-- lpeg
-----------------------------------------------------------------------------------
local lpeg = require"lpeg"

-- common pattern
local blank = (lpeg.P(" ")+lpeg.P("\t"))^1
local empty = blank^0
local blocksplit = (blank+lpeg.P("\n"))^1
local alpha = lpeg.R("az", "AZ")
local digit = lpeg.R("09")
local head_char = alpha + lpeg.S("_")
local var_name = (head_char * (head_char+digit)^0)
local scomment = lpeg.P("//")*(1-lpeg.S("\n"))^0*lpeg.S("\n")
local mcomment = lpeg.P("/*")*(1-(lpeg.P("/*")+lpeg.P("*/")))^0*lpeg.P("*/")
local comment = blocksplit^0*(scomment+mcomment)*blocksplit^0 + blocksplit
local expr = (empty * lpeg.S("+-")^-1 * empty * (digit^1 + var_name))^1

-- field pattern
local field_label = (lpeg.P("required") + "optional" + "repeated") / getfield_label
local field_type = var_name/getfield_type
local field_name = var_name/getfield_name
local field_order = (digit^1)/getfield_order
local field_value = (digit^1)/getfield_value
local field_comment = comment/getfield_comment
local fix_repeat = lpeg.P("[") * 
        empty*(expr/getfield_fixrepeat) *
        empty*lpeg.P("]")
local var_repeat = lpeg.P("[") *
        empty*lpeg.P("max") *
        empty*lpeg.P("=") *
        empty*(expr/getfield_varrepeat) *
        empty*lpeg.P("]")
local field = field_label * 
		empty * field_type * 
		empty * field_name *
        empty * ((fix_repeat^-1))*
	    empty * "=" *
		empty * field_order *
		empty * lpeg.P(";")
local field2 = field_label^-1 * 
		empty * field_type * 
		empty * field_name *
        empty * ((var_repeat^-1))*
	    empty * "=" *
		empty * field_order *
		empty * lpeg.P(";")

local field_line = (field+field2)/getmfield + field_comment

-- fenum pattern
local enum_value = lpeg.P("=")*empty*((digit^1 + var_name)/getfield_value)
local fenum = field_name * 
		empty * enum_value^-1 *
		empty * (lpeg.P(";") + lpeg.P(","))
local fenum_line = fenum/getefield + field_comment
		

local block_name = var_name
local block_comment = comment/getblock_comment
local message = ((lpeg.P("message")+lpeg.P("struct")) * blank * (block_name/getmessage) * blocksplit *
				"{" * field_line^1 * "}" * lpeg.P(";")^0)/endmessage
local enum = (lpeg.P("enum")* blank * (block_name/getenum) * blocksplit *
		"{" * fenum_line^1 * "}" * lpeg.P(";")^0)/endenum
local enum_uname = (lpeg.P("enum")/getenum_unname* 
        (blank * block_name/getenum_name)^-1 * blocksplit *
		"{" * fenum_line^1 * "}" * lpeg.P(";")^0)/endenum

local import = lpeg.P("import") * blank * (block_name/getimport)
local pack = lpeg.P("#pragma") * blank * lpeg.P("pack") * 
        empty * lpeg.P("(") * 
        empty * digit^-1 * 
        empty * lpeg.P(")")
local block_pack = pack/getblock_pack
local block = block_comment+import+message+enum + enum_uname + block_pack

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

local function _dump_instruction()
    io.write('/*this file is generate by protoc.lua do not change it by hand*/\n')
end

local function _dump_h_header(outname)
    io.write(string.format("#ifndef __%s_H__\n", string.upper(outname)))
    io.write(string.format("#define __%s_H__\n", string.upper(outname)))
end

local function _dump_h_tail()
    io.write(string.format("#endif"))
end

local function _dump_field(field) 
    if field.sign == "comment" then
        io.write(field.value)
    elseif field.sign == "mfield" then
        local rep = ""
        if field.fixrepeat ~= "" then 
            rep = string.format("[%s]", field.fixrepeat)
        elseif field.varrepeat ~= "" then
            io.write(string.format("uint16_t n%s;\n    ", field.name));
            rep = string.format("[%s]", field.varrepeat)
        end
        local ftype = _TYPES[field.type]
        if not ftype then
            ftype = "struct " .. field.type
        end
        io.write(string.format("%s %s%s;", ftype, field.name, rep))
    elseif field.sign == "efield" then
        local value = ""
        if field.value ~= "" then
            value = string.format(" = %s", field.value)
        end
        io.write(string.format("%s%s,", field.name, value))
    end
end

local function _dump_block(blocktable, name) 
    _dump_instruction()
    _dump_h_header(string.format(string.upper(name).."_PB"))
    io.write('#include <stdint.h>\n\n')
    for _, block in ipairs(blocktable) do
        if block.sign == "import" then
            io.write(string.format('#include "%s.pb.h"', block.name))
        elseif block.sign == "comment" or
               block.sign == "pragma pack" then
            io.write(string.format(block.name))
        elseif block.sign == "message" then
            io.write(string.format("struct %s {", block.name))
            for _, field in ipairs(block.fields) do
                _dump_field(field)
            end
            io.write("};")
        elseif block.sign == "enum" then
            io.write(string.format("enum %s {", block.name))
            for _, field in ipairs(block.fields) do
                _dump_field(field)
            end
            io.write("};")
        end
    end
    _dump_h_tail()
end

local function _serialize_field_decl(mname, field, blocktable) 
    local order = string.match(field.order, "%d+")+0
    assert(order > 0 and order <= 4096, 
        string.format("%s::%s number must be 1~4096", mname, field.name))

    local base_type = not (not _TYPES[field.type]) 
    if not base_type then
        local msg_type = false
        for _, b in ipairs(blocktable) do
            if b.sign == "message" then
                if b.name == field.type then
                    msg_type = true
                    break
                end
            end
        end
        assert(msg_type, string.format("%s unknown field type:%s", mname, field.type))
    end
    
    local bytes = "0"
    if not base_type then
        bytes = string.format("sizeof(struct %s)", field.type)
    end
   
    local repeat_max = "0"
    local repeat_offset = "0"
    if field.fixrepeat ~= "" then
        repeat_max = field.fixrepeat
    end
    if field.varrepeat ~= "" then
        assert(field.label == "repeated", 
            string.format("%s::%s label must be repeated", mname, field.name))
        local align = string.rep(" ", 9)
        repeat_offset = string.format("\n%soffsetof(struct %s, %s) - offsetof(struct %s, n%s)",
            align, mname, field.name, mname, field.name)
        repeat_max = field.varrepeat
    end

    local label = _LABELS[field.label]
    assert(label, string.format("%s::%s unknown label:%s", mname, field.name, field.label))
    local ftype = string.format("%s%s", label, field.type)
    
    return string.format('"%s", %s, offsetof(struct %s, %s), %s, %s, %s, "%s"', 
        field.name, field.order, mname, field.name, bytes, repeat_max, repeat_offset, ftype)
end

local function _dump_mfields(m, blocktable)
    for _, field in ipairs(m.fields) do
        if field.sign == "mfield" then
            io.write(string.format(
            "        {%s},\n", _serialize_field_decl(m.name, field, blocktable)))
        end
    end
end

local function _pbo_name(name)
    return string.format("PBO_%s", string.upper(name))
end

local function _dump_message(m, n, blocktable)
    io.write(string.format(
             "    struct pb_field_decl fds%d[] = {\n", n))
    _dump_mfields(m, blocktable)
    io.write("    };\n")
    io.write(string.format(
             '    %s = pb_context_object(pbc, "%s", fds%d, sizeof(fds%d)/sizeof(fds%d[0]));\n', 
                                              _pbo_name(m.name), m.name, n, n, n))
    io.write(string.format(
            "    if (%s == NULL) {\n", _pbo_name(m.name)))
    io.write('        PB_LOG("pb object error: %s", pb_context_lasterror(pbc));\n')
    io.write("        pb_context_delete(pbc);\n")
    io.write("        return NULL;\n")
    io.write("    }\n\n")
end

local function _dump_messages(blocktable)
    local i = 1 
    for _, b in ipairs(blocktable) do
        if b.sign == "message" then
            _dump_message(b, i, blocktable)
            i = i+1
        end
    end
end

local function _get_messagecount(blocktable)
    local nmessage = 0;
    for _, block in ipairs(blocktable) do
        if block.sign == "message" then
            nmessage = nmessage + 1
        end
    end
    return nmessage
end
local function _dump_includes(defines)
    io.write('#include <stddef.h>\n')
    io.write('#include "pb.h"\n')
    io.write('#include "pb_log.h"\n')
    for _, def in ipairs(defines) do
        io.write(string.format('#include "%s.pb.h"\n', def))
    end
end
local function _dump_pbobject_decl(blocktable)
    for _, b in ipairs(blocktable) do
        if b.sign == "message" then
            io.write(string.format("struct pb_object* %s = NULL;\n", _pbo_name(b.name)))
        end
    end
end
local function _dump_context(blocktable, defines, outname) 
    _dump_instruction()
    _dump_h_header(outname)
    _dump_includes(defines)
    _dump_pbobject_decl(blocktable) 
    local nmessage = _get_messagecount(blocktable)
    io.write("struct pb_context*\n")
    io.write("PB_CONTEXT_INIT() {\n")
    io.write(string.format(
             "    struct pb_context* pbc = pb_context_new(%d);\n", nmessage))
    io.write("    if (pbc == NULL) {\n")
    io.write("        return NULL;\n")
    io.write("    }\n")
    _dump_messages(blocktable)
    io.write("    pb_context_fresh(pbc);\n");
    io.write("    if (pb_context_verify(pbc)) {\n");
    io.write('        PB_LOG("pb verify error: %s", pb_context_lasterror(pbc));\n')
    io.write("        return NULL;\n")
    io.write("    }\n")
    io.write("    return pbc;\n");
    io.write("}\n\n");
    _dump_h_tail();
end

-------------------------------------------------------------------------------
-- locate error
-------------------------------------------------------------------------------
local function locate_error(content, err_pos)
    local poses = {}
    local pos = 0
    local line = 0
    repeat
        local last_pos = pos
        pos = string.find(content, '\n', pos + 1)
        line = line + 1
        if not pos or pos > err_pos then
            return line, err_pos - last_pos
        end
    until not pos
end

local function show_error(content, err_pos)
    local row, col = locate_error(content, err_pos)
    local head = string.format("[cexport error report][row:%d col:%d]", row, col)
    print(string.rep('#', string.len(head)))
    print(head)
    print(string.rep('#', 79))
    local newline, _ = string.find(content, '\n', err_pos)
    if newline then
        print(string.sub(content, err_pos, newline-1))
    else
        print(string.sub(content, err_pos))
    end
    print(string.rep('#', 79))
end

-------------------------------------------------------------------------------
-- parser
-------------------------------------------------------------------------------
local plp = {}

function plp.block_new()
    return {}
end

function plp.parse_string(str, blockout)
	block_cache = blockout
	local pos = 1
    local last_pos
	while pos do
        last_pos = pos
		pos = lpeg.match(block, str, pos)
    end
    if last_pos - 1 == string.len(str) then
        return true
    else
        show_error(str, last_pos)
        return false
    end
end

function plp.parse_file(fname, blockout)
    print(string.format('[parsing file %s]', fname))
	local file = io.open(fname, "r")
	local strfile = file:read("*a")
	file:close()
	return plp.parse_string(strfile, blockout)
end

function plp.dump(block, name, out)
    local old_output = io.output()
    if out then
        io.output(out)
    end
    _dump_block(block, name)
    io.output(old_output)
end
function plp.dump_context(blocktable, defines, out, outname)
    local old_output = io.output()
    if out then
        io.output(out)
    end
    _dump_context(blocktable, defines, outname)
    io.output(old_output)
end
return plp
