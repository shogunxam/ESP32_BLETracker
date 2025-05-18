import re
import os
import gzip
import re
import argparse
import shutil
from pathlib import Path
import importlib.util
import sys
import subprocess

def check_and_install_requirements(requirements):
    for package in requirements:       
        if  importlib.util.find_spec(package) is None:
            print(f"Installation of {package}...")
            subprocess.check_call([sys.executable, "-m", "pip", "install", package])
            print(f"{package} installed successfully.")


check_and_install_requirements(["htmlmin", "csscompressor", "jsmin"])

try:    
    import htmlmin
    import csscompressor
    from jsmin import jsmin
except ImportError as e:
    print(f"Error importing the modules {e}")
    sys.exit(1)

def extract_version_from_header(header_file_path):
    try:
        with open(header_file_path, 'r') as file:
            content = file.read()
            
            # Pattern per trovare la definizione #define VERSION "x.y.z"
            pattern = r'#define\s+VERSION\s+"([^"]+)"'
            match = re.search(pattern, content)
            
            if match:
                version = match.group(1)
                return version
            else:
                return "Version not found"
    except Exception as e:
        return f"Error reading file: {str(e)}"   

try:
     # This is for PlatformIO
    Import("env")
    version = extract_version_from_header(env.get("PROJECT_SRC_DIR") + "/firmwarever.h")
    print("Version: ", version)
    firmware_name = env.GetProjectOption("custom_firmware_name")
    env.Replace(PROGNAME="%s-firmware-%s" % (firmware_name, version))
except:
   pass


def minimize_html(content):
    return htmlmin.minify(content, 
                          remove_comments=True,
                          remove_empty_space=True,
                          remove_all_empty_space=True,
                          reduce_boolean_attributes=True)


def minimize_css(content):
    return csscompressor.compress(content)


def minimize_js(content):
    return jsmin(content)


def compress_file(input_path, output_path):
    with open(input_path, 'rb') as f_in:
        with gzip.GzipFile(filename=output_path, mode='wb', compresslevel=9, mtime=0) as f_out:
            shutil.copyfileobj(f_in, f_out)

    original_size = os.path.getsize(input_path)
    compressed_size = os.path.getsize(output_path)
    compression_ratio = (1 - (compressed_size / original_size)) * 100
    
    return original_size, compressed_size, compression_ratio

def bin2c(filename, outfilename):
    varname = os.path.basename(filename)
    varname=varname.replace('.','_')
    varname_size = varname+'_size'
    with open(outfilename,'wb') as result_file:
        result_file.write(b"//Don't edit this file, it's automatically generated\n")
        result_file.write(b'static const long int %s = %d;\n' % (varname_size.encode('utf-8'), os.stat(filename).st_size))
        result_file.write(b'static const unsigned char %s[] = {\n' % varname.encode('utf-8'))
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

def process_files(directory, output_dir=None, verbose=True):
    if not output_dir:
        output_dir = directory
    
    os.makedirs(output_dir, exist_ok=True)
    
    total_original = 0
    total_minified = 0
    total_compressed = 0
    
    print(f"Searching file in: {directory}")
    for root, _, files in os.walk(directory):
        for file in files:
            file_path = os.path.join(root, file)
            file_ext = os.path.splitext(file)[1].lower()
            
            if file.endswith('.min') or file.endswith('.gz'):
                continue
                
            if file_ext in ['.html', '.css', '.js']:
                try:
                    with open(file_path, 'r', encoding='utf-8') as f:
                        content = f.read()
                    
                    if file_ext == '.html':
                        minified_content = minimize_html(content)
                    elif file_ext == '.css':
                        minified_content = minimize_css(content)
                    elif file_ext == '.js':
                        minified_content = minimize_js(content)
                    
                    rel_path = os.path.relpath(file_path, directory)
                    min_file = os.path.join(output_dir, rel_path)
                    gz_file = min_file + '.gz'
                    
                    os.makedirs(os.path.dirname(min_file), exist_ok=True)
                    
                    min_file = min_file + '.min'

                    with open(min_file, 'w', encoding='utf-8') as f:
                        f.write(minified_content)
                    
                    orig_size, min_size, compression = compress_file(min_file, gz_file)

                    h_file = gz_file+'.h'
                    bin2c(gz_file,h_file)
                    os.remove(min_file)
                    os.remove(gz_file)
                    
                    if verbose:
                        print(f"File: {rel_path}")
                        print(f"  Original: {len(content)} bytes")
                        print(f"  Minimized: {len(minified_content)} bytes (-{(1 - len(minified_content)/len(content))*100:.1f}%)")
                        print(f"  Compressed: {min_size} bytes (-{compression:.1f}%)")
                        print("")
                    
                    total_original += len(content)
                    total_minified += len(minified_content)
                    total_compressed += min_size
                    
                except Exception as e:
                    print(f"Error elaborating {file_path}: {str(e)}")
    
    if total_original > 0:
        print("\nStats:")
        print(f"Total original: {total_original} bytes")
        print(f"Total minimized: {total_minified} bytes (-{(1 - total_minified/total_original)*100:.1f}%)")
        print(f"Total compressed: {total_compressed} bytes (-{(1 - total_compressed/total_original)*100:.1f}%)")
    else:
        print("No HTML, CSS o JS file found.")


process_files('./main/html', './main/html', True)
