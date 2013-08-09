
---------------------------------------------------------------------------------------------
-- 文件操作相关
---------------------------------------------------------------------------------------------
local lfs = require"lfs"

local fs = {}
-- 获取目录path下ext类型文件
function fs.getfiles(path, ext)
	local files = {}
	local init = -(#ext)
	local getattr = lfs.attributes
	for file in lfs.dir(path) do
		if file ~= "." and file ~= ".." then
			local fname = path .. '/' .. file
			if getattr(fname).mode == "file" and string.find(fname, ext, init, true) then
				files[#files+1] = fname
			end
		end	
	end
	return files
end

return fs
