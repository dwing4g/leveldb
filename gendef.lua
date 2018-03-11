local f = io.open("include/leveldb/c.h", "rb")
local s = f:read "*a"
f:close()

local names = {}
for name in s:gmatch("\nLEVELDB_EXPORT%s+[%w _*]+%s+([%w_]+)%s*%(") do
	names[#names + 1] = name
end
table.sort(names)

f = io.open("leveldb.def", "wb")
f:write("EXPORTS\r\n")
f:write("Java_jane_core_StorageLevelDB_leveldb_1backup\r\n")
f:write("Java_jane_core_StorageLevelDB_leveldb_1close\r\n")
f:write("Java_jane_core_StorageLevelDB_leveldb_1compact\r\n")
f:write("Java_jane_core_StorageLevelDB_leveldb_1get\r\n")
f:write("Java_jane_core_StorageLevelDB_leveldb_1iter_1delete\r\n")
f:write("Java_jane_core_StorageLevelDB_leveldb_1iter_1new\r\n")
f:write("Java_jane_core_StorageLevelDB_leveldb_1iter_1next\r\n")
f:write("Java_jane_core_StorageLevelDB_leveldb_1iter_1prev\r\n")
f:write("Java_jane_core_StorageLevelDB_leveldb_1iter_1value\r\n")
f:write("Java_jane_core_StorageLevelDB_leveldb_1open\r\n")
f:write("Java_jane_core_StorageLevelDB_leveldb_1open2\r\n")
f:write("Java_jane_core_StorageLevelDB_leveldb_1open3\r\n")
f:write("Java_jane_core_StorageLevelDB_leveldb_1property\r\n")
f:write("Java_jane_core_StorageLevelDB_leveldb_1write\r\n")
f:write("Java_jane_core_StorageLevelDB_leveldb_1write_1direct\r\n")
for _, name in ipairs(names) do
	f:write(name, "\r\n")
end
f:close()

print "done!"
