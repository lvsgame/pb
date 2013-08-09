require "ptable"
local lpeg = require "lpeg"
local s = "requiredrequired"
--local label = (lpeg.P("required") * lpeg.Cc("REQUIRE"))^1
--local label = (lpeg.C(lpeg.P("required")))^1
local label = lpeg.Ct(lpeg.C("required")^1)
local r = lpeg.match(label, s)
print (r)
for k,v in pairs(r) do
    print(k, v)
end

print_table(r, "r")

function split(s, sep)
    sep = lpeg.P(sep)
    local elem = lpeg.C((1 - sep)^0)
    local p = lpeg.Ct(elem * (sep * elem)^0)
    return lpeg.match(p, s)
end

r = split("ab;cd;ef;gh", ";")
print (r)
print_table(r, "r")

local buffer = [[
message Test {
    required int32 i1 = 1;
    optional int32 i2 = 2;
    repeated int32 i3 = 3;
}
]]

local P = lpeg.P
local S = lpeg.S
local R = lpeg.R
local C = lpeg.C
local Cg = lpeg.Cg
local Ct = lpeg.Ct

local newline = P("\n") + "\r\n"
local blank = S(" \t") + newline
local blanks = blank^1
local empty = blank^0
local digit = R("09")
local alpha = R("az", "AZ")
local varname = (alpha + "_") * (alpha + "_" + digit)^0

local scomment = P("//") * (1 - newline)^0 * newline
local mcomment = P("/*") * (1 - (P("/*")+"*/"))^0 * P("*/")
local comment = scomment + mcomment + blanks

local label = P("required") + "optional" + "repeated"
local ptype = varname
local number = digit^1

local field = Cg(C(label),  * blanks * varname * blanks * varname * blanks * P(";")
local message = P("message") * blanks * varname * blanks * P("{") *
    (field + comment)^0 *
    blanks * P("}")
local pattern = (message + comment)^0

print"--------------begin--------------"
local r = lpeg.match(pattern, buffer)
print(r)
