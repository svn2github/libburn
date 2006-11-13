# a module to help with handling of filenames, directory trees, etc.

import os
import os.path
import stat

def pathsubtract(a, b):
    index = a.find(b)
    if index == -1:
        return None
    res = a[ (index + len(b)): ]

    if res.find("/") == 0:
        res = res[1:]
    return res

# same as C strcmp()
def strcmp(a, b):
    if a < b:
        return -1
    if a > b:
        return 1
    return 0

class TreeNode:

    # path is the location of the file/directory. It is either a full path or
    # a path relative to $PWD
    def __init__(self, parent, path=".", root=".", isofile=None):
        if isofile:
            self.root = os.path.abspath(isofile)
            self.path = ""
        else:
            fullpath = os.path.abspath( path )
            fullroot = os.path.abspath( root )
            self.root = fullroot
            self.path = pathsubtract( fullpath, fullroot )
        self.parent = parent
        self.children = []

        if self.path == None:
            raise NameError, "Invalid paths %s and %s" % (fullpath, fullroot)

    # if this is a directory, add its children recursively
    def addchildren(self):
        if not stat.S_ISDIR( os.lstat(self.root + "/" + self.path).st_mode ):
            return

        children = os.listdir( self.root + "/" + self.path )
        for child in children:
            if self.path:
                child = self.path + "/" + child
            self.children.append( TreeNode(self, child, self.root) )
        for child in self.children:
            child.addchildren()

    def printAll(self, spaces=0):
        print " "*spaces + self.root + "/" + self.path
        for child in self.children:
            child.printAll(spaces + 2)

    def isValidISO1(self):
        pass

class Tree:
    def __init__(self, root=None, isofile=None):
        if isofile:
            self.root = TreeNode(parent=None, isofile=isofile)
        else:
            self.root = TreeNode(parent=None, path=root, root=root)
            self.root.addchildren()

    def isValidISO1(self):
        return root.isValidISO1();

#t = Tree(root=".")
#t.root.printAll()
