Import("env")
import gzip
import ntpath
import os
import sys

def compress(filename):
    outfilename = filename+'.gz'
    with open(filename, 'rb') as src, gzip.open(outfilename, 'wb') as dst:
        dst.writelines(src)
    return outfilename

def bin2c(filename):
    outfilename = filename+'.h'
    varname = ntpath.basename(filename)
    varname=varname.replace('.','_')
    varname_size = varname+'_size'
    with open(outfilename,'wb') as result_file:
        result_file.write(b"//Don't edit this file it's automatically generated\n")
        result_file.write(b'const long int %s = %d;\n' % (varname_size.encode('utf-8'), os.stat(filename).st_size))
        result_file.write(b'const unsigned char %s[] = {\n' % varname.encode('utf-8'))
        counter = 0
        first = True
        for b in open(filename, 'rb').read():
            counter = counter + 1
            if counter == 16:
                result_file.write(b'\n')
                counter = 0
            if first:
                first = False
            else:
                result_file.write(b',')
            result_file.write(b'0x%02X' % b)
        result_file.write(b'\n};')

#with os.scandir(sys.argv[1]) as it :
with os.scandir('./main/html') as it :
    for entry in it:
        if entry.is_file() and entry.name.endswith('.html') or  entry.name.endswith('.css') or entry.name.endswith('.js'):
            outfile = compress(entry.path)
            bin2c(outfile)
            os.remove(outfile)