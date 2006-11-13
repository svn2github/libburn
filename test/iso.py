
import struct
import tree
import sys

voldesc_fmt = "B" "5s" "B" "2041x"

# all these fields are common between the pri and sec voldescs
privoldesc_fmt = "B" "5s" "B" "x" "32s" "32s" "8x" "8s" "32x" "4s" "4s" "4s" "8s" "4s4s" "4s4s" "34s" "128s" \
    "128s" "128s" "128s" "37s" "37s" "37s" "17s" "17s" "17s" "17s" "B" "x" "512s" "653x"

# the fields unique to the sec_vol_desc
secvoldesc_fmt = "x" "5x" "x" "B" "32x" "32x" "8x" "8x" "32s" "4x" "4x" "4x" "8x" "4x4x" "4x4x" "34x" "128x" \
    "128x" "128x" "128x" "37x" "37x" "37x" "17x" "17x" "17x" "17x" "x" "x" "512x" "653x"

dirrecord_fmt = "B" "B" "8s" "8s" "7s" "B" "B" "B" "4s" "B" # + file identifier, padding field and SU area

pathrecord_fmt = "B" "B" "4s" "2s" # + directory identifier and padding field

def read_bb(str, le, be):
    val1, = struct.unpack(le, str)
    val2, = struct.unpack(be, str)
    if val1 != val2:
        print "val1=%d, val2=%d" % (val1, val2)
        raise AssertionError, "values are not equal in dual byte-order field"
    return val1

def read_bb4(str):
    return read_bb(str, "<I4x", ">4xI")

def read_bb2(str):
    return read_bb(str, "<H2x", ">2xH")

def read_lsb4(str):
    return struct.unpack("<I", str)[0]

def read_lsb2(str):
    return struct.unpack("<H", str)[0]

def read_msb4(str):
    return struct.unpack(">I", str)[0]

def read_msb2(str):
    return struct.unpack(">H", str)[0]

class VolDesc(object):
    def __init__(self, data):
        print "fmt len=%d, data len=%d" % ( struct.calcsize(voldesc_fmt), len(data) )
        self.vol_desc_type, self.standard_id, self.vol_desc_version = struct.unpack(voldesc_fmt, data)

class PriVolDesc(VolDesc):
    def __init__(self, data):
        self.vol_desc_type, \
        self.standard_id, \
        self.vol_desc_version, \
        self.system_id, \
        self.volume_id, \
        self.vol_space_size, \
        self.vol_set_size, \
        self.vol_seq_num, \
        self.block_size, \
        self.path_table_size, \
        self.l_table_pos, \
        self.l_table2_pos, \
        self.m_table_pos, \
        self.m_table2_pos, \
        self.root_record, \
        self.volset_id, \
        self.publisher_id, \
        self.preparer_id, \
        self.application_id, \
        self.copyright_file, \
        self.abstract_file, \
        self.bibliographic_file, \
        self.creation_timestamp, \
        self.modification_timestamp, \
        self.expiration_timestamp, \
        self.effective_timestamp, \
        self.file_struct_version, \
        self.application_use = struct.unpack(privoldesc_fmt, data)

        # take care of reading the integer types
        self.vol_space_size = read_bb4(self.vol_space_size)
        self.vol_set_size = read_bb2(self.vol_set_size)
        self.vol_seq_num = read_bb2(self.vol_seq_num)
        self.block_size = read_bb2(self.block_size)
        self.path_table_size = read_bb4(self.path_table_size)
        self.l_table_pos = read_lsb4(self.l_table_pos)
        self.l_table2_pos = read_lsb4(self.l_table2_pos)
        self.m_table_pos = read_msb4(self.m_table_pos)
        self.m_table2_pos = read_msb4(self.m_table2_pos)

        # parse the root directory record
        self.root_record = DirRecord(self.root_record)

    def readPathTables(self, file):
        file.seek( self.block_size * self.l_table_pos )
        self.l_table = PathTable( file.read(self.path_table_size), 0 )
        file.seek( self.block_size * self.m_table_pos )
        self.m_table = PathTable( file.read(self.path_table_size), 1 )

        if self.l_table2_pos:
            file.seek( self.block_size * self.l_table2_pos )
            self.l_table2 = PathTable( file.read(self.path_table_size), 0 )
        else:
            self.l_table2 = None

        if self.m_table2_pos:
            file.seek( self.block_size * self.m_table2_pos )
            self.m_table2 = PathTable( file.read(self.path_table_size), 1 )
        else:
            self.m_table2 = None

    def toTree(self, isofile):
        ret = tree.Tree(isofile=isofile.name)
        ret.root = self.root_record.toTreeNode(parent=None, isofile=isofile)
        return ret

class SecVolDesc(PriVolDesc):
    def __init__(self, data):
        super(SecVolDesc,self).__init__(data)
        self.flags, self.escape_sequences = struct.unpack(secvoldesc_fmt, data)

# return a single volume descriptor of the appropriate type
def readVolDesc(data):
    desc = VolDesc(data)
    if desc.standard_id != "CD001":
        print "Unexpected standard_id " +desc.standard_id
        return None
    if desc.vol_desc_type == 1:
        return PriVolDesc(data)
    elif desc.vol_desc_type == 2:
        return SecVolDesc(data)
    elif desc.vol_desc_type == 3:
        print "I don't know about partitions yet!"
        return None
    elif desc.vol_desc_type == 255:
        return desc
    else:
        print "Unknown volume descriptor type %d" % (desc.vol_desc_type,)
        return None

def readVolDescSet(file):
    ret = [ readVolDesc(file.read(2048)) ]
    while ret[-1].vol_desc_type != 255:
        ret.append( readVolDesc(file.read(2048)) )

    for vol in ret:
        if vol.vol_desc_type == 1 or vol.vol_desc_type == 2:
            vol.readPathTables(file)

    return ret

class DirRecord:
    def __init__(self, data):
        self.len_dr, \
        self.len_xa, \
        self.block, \
        self.len_data, \
        self.timestamp, \
        self.flags, \
        self.unit_size, \
        self.gap_size, \
        self.vol_seq_number, \
        self.len_fi = struct.unpack(dirrecord_fmt, data[:33])
        self.children = []

        if self.len_dr > len(data):
            raise AssertionError, "Error: not enough data to read in DirRecord()"
        elif self.len_dr < 34:
            raise AssertionError, "Error: directory record too short"

        fmt = str(self.len_fi) + "s"
        if self.len_fi % 2 == 0:
            fmt += "1x"
        len_su = self.len_dr - (33 + self.len_fi + 1 - (self.len_fi % 2))
        fmt += str(len_su) + "s"

        if len(data) >= self.len_dr:
            self.file_id, self.su = struct.unpack(fmt, data[33 : self.len_dr])
        else:
            print "Error: couldn't read file_id: not enough data"
            self.file_id = "BLANK"
            self.su = ""

        # convert to integers
        self.block = read_bb4(self.block)
        self.len_data = read_bb4(self.len_data)
        self.vol_seq_number = read_bb2(self.vol_seq_number)

    def toTreeNode(self, parent, isofile, path=""):
        ret = tree.TreeNode(parent=parent, isofile=isofile.name)
        if len(path) > 0:
            path += "/"
        path += self.file_id
        ret.path = path

        if self.flags & 2: # we are a directory, recurse
            isofile.seek( 2048 * self.block )
            data = isofile.read( self.len_data )
            pos = 0
            while pos < self.len_data:
                try:
                    child = DirRecord( data[pos:] )
                    pos += child.len_dr
                    if child.len_fi == 1 and (child.file_id == "\x00" or child.file_id == "\x01"):
                        continue
                    print "read child named " +child.file_id
                    self.children.append( child )
                    ret.children.append( child.toTreeNode(ret, isofile, path) )
                except AssertionError:
                    print "Couldn't read child of directory %s, position is %d, len is %d" % \
                            (path, pos, self.len_data)
                    raise

        return ret

class PathTableRecord:
    def __init__(self, data, readint2, readint4):
        self.len_di, self.len_xa, self.block, self.parent_number = struct.unpack(pathrecord_fmt, data[:8])

        if len(data) < self.len_di + 8:
            raise AssertionError, "Error: not enough data to read path table record"

        fmt = str(self.len_di) + "s"
        self.dir_id, = struct.unpack(fmt, data[8:8+self.len_di])

        self.block = readint4(self.block)
        self.parent_number = readint2(self.parent_number)

class PathTable:
    def __init__(self, data, m_type):
        if m_type:
            readint2 = read_msb2
            readint4 = read_msb4
        else:
            readint2 = read_lsb2
            readint4 = read_lsb4
        pos = 0
        self.records = []
        while pos < len(data):
            try:
                self.records.append( PathTableRecord(data[pos:], readint2, readint4) )
                print "Read path record %d: dir_id %s, block %d, parent_number %d" %\
                        (len(self.records), self.records[-1].dir_id, self.records[-1].block, self.records[-1].parent_number)
                pos += self.records[-1].len_di + 8
                pos += pos % 2
            except AssertionError:
                print "Last successfully read path table record had dir_id %s, block %d, parent_number %d" % \
                        (self.records[-1].dir_id, self.records[-1].block, self.records[-1].parent_number)
                print "Error was near offset %x" % (pos,)
                raise

    def findRecord(self, dir_id, block, parent_number):
        number=1
        for record in self.records:
            if record.dir_id == dir_id and record.block == block and record.parent_number == parent_number:
                return number, record
            number += 1

        return None, None

    # check this path table for consistency against the actual directory heirarchy
    def crossCheckDirRecords(self, root, parent_number=1):
        number, rec = self.findRecord(root.file_id, root.block, parent_number)

        if not rec:
            print "Error: directory record parent_number %d, dir_id %s, block %d doesn't match a path table record" % \
                    (parent_number, root.file_id, root.block)
            parent = self.records[parent_number]
            print "Parent has parent_number %d, dir_id %s, block %d" % (parent.parent_number, parent.dir_id, parent.block)
            return 0

        for child in root.children:
            if child.flags & 2:
                self.crossCheckDirRecords(child, number)


if len(sys.argv) != 2:
    print "Please enter the name of the .iso file to open"
    sys.exit(1)

f = file(sys.argv[1])
f.seek(2048 * 16) # system area
volumes = readVolDescSet(f)
vol = volumes[0]
t = vol.toTree(f)
vol.l_table.crossCheckDirRecords(vol.root_record)
vol.m_table.crossCheckDirRecords(vol.root_record)

vol = volumes[1]
try:
	t = vol.toTree(f)
	vol.l_table.crossCheckDirRecords(vol.root_record)
	vol.m_table.crossCheckDirRecords(vol.root_record)
except AttributeError:
	pass
