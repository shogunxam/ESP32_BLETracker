try:
     # This is for PlatformIO
    Import("env")
except:
   pass

import gzip
import os
import sys

'''
try:
    from css_html_js_minify import process_single_html_file, process_single_js_file, process_single_css_file, html_minify, js_minify, css_minify
except ImportError:
    env.Execute("$PYTHONEXE -m pip install -vvv css_html_js_minify")

from css_html_js_minify import process_single_html_file, process_single_js_file, process_single_css_file, html_minify, js_minify, css_minify

def minify(filename, outfilename):
    _ , file_extension = os.path.splitext(filename)
    file_extension = file_extension.lower()

    if file_extension == '.html' or file_extension == '.htm':
        process_single_html_file(filename,output_path=outfilename)
    elif file_extension == '.css':
        process_single_css_file(filename,output_path=outfilename)
    elif file_extension == '.js':
        process_single_js_file(filename,output_path=outfilename)
'''

def compress(filename, outfilename):
    with open(filename, 'rb') as src, gzip.open(outfilename, 'wb') as dst:
        dst.writelines(src)

def bin2c(filename, outfilename):
    varname = os.path.basename(filename)
    varname=varname.replace('.','_')
    varname_size = varname+'_size'
    with open(outfilename,'wb') as result_file:
        result_file.write(b"//Don't edit this file, it's automatically generated\n")
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

with os.scandir('./main/html') as it :
    for entry in it:
        if entry.is_file() and entry.name.endswith('.html') or  entry.name.endswith('.css') or entry.name.endswith('.js'):
            '''
            min_file = entry.path+'.min'
            minify(entry.path,min_file)
            '''

            gz_file=entry.path+'.gz'
            compress(entry.path, gz_file)
            h_file = gz_file+'.h'
            bin2c(gz_file,h_file)
            '''os.remove(min_file)'''
            os.remove(gz_file)