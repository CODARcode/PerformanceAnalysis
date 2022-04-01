#Description: A rudimentary compiler-free C++ autoinstrumentor (it assumes the code is valid C++, use a real compiler first if unsure!)
#Author: Christopher Kelly  (ckelly@bnl.gov)
#Notes: Script requires sctokenizer (https://pypi.org/project/sctokenizer/) and generating the instrumented binary requires Perfstubs API (https://github.com/khuck/perfstubs)
#Script usage: python3 instrument_file_cpp.py <filename>      ->  <filename>.inst

# For a project you can build a bash script to loop over the files in a source directory, e.g. for Grid
#  #!/bin/bash
#
#  for dir in Grid benchmarks; do
#    echo $dir
#    files=$(find $dir -type f \( -iname "*.h" -o -iname "*.cc" \) )
#    for file in $files; do
#        if [[ $file =~ 'Grid/simd' || $file =~ 'Grid/pugixml' || $file =~ 'Grid/Eigen'  ]]; then
#            echo "Skipping $file"
#            continue
#        fi
#        echo "Instrumenting $file"
#
#        python3.6 instrument_file.py $file
#        mv "${file}.inst" $file
#    done
#  done

import sctokenizer
import sys
import re

class Function:
    def __init__(self, name, args, open_line_col, close_line_col):
        self.name = name
        self.args = args
        self.open_line_col = open_line_col
        self.close_line_col = close_line_col
        

#From start, try to collect the class identifier        
def rewind_class_name(start, tokens):
    trib_depth = 0
    if tokens[start-1].token_value == '::':
        i = start-2
        while(1):
            if tokens[i].token_value == '>':
                trib_depth += 1
            elif tokens[i].token_value == '<':
                trib_depth -= 1

            if tokens[i].token_type == sctokenizer.TokenType.IDENTIFIER and trib_depth == 0:                
                break
            i-=1
        cname=""
        for j in range(i,start-1):
            cname = cname + tokens[j].token_value
        return cname
    else:
        return None

#Return (true, token index) if the identifier is preceded by any of the strings in keywords    ,  (false, None) otherwise
def func_keyword_check(start, tokens, keywords):
    i=start-1    
    while(i>=0):
        tok = tokens[i]
        print("BACK SEARCH",tok.token_value)
        if tok.token_value in ['}',';']:        
            print("BACK SEARCH EXIT ON", tok.token_value)
            return (False, None)
        elif tok.token_value in keywords:
            return (True, i)
        else:
            i-=1
    return (False, None)
    
filename = sys.argv[1]
print(filename)

all_tokens = sctokenizer.tokenize_file(filepath=filename, lang='cpp')

in_preproc = False #Are we inside a preprocessor statement?
preproc_prev_line = None #line number in a define. Usually this is the same line as the define originated unless there is line continuance, '\'

fname="" #current function name
fargs="" #function arguments
func_arg_open=False  #are we testing for a function arguments
func_postargs_open=False #are we between function arguments and open braces?
func_postargs_initializer_list=False #function is a constructor and has an initializer list
func_postargs_auto_type=False #function has an auto return type eg  auto func()-> blah {
func_body_open=False #are we inside a function body?
func_paren_depth=None #what paren depth will the close parentheses be at?
func_body_brace_depth=None #what brace depth will the body close braces be at?
func_body_open_line_col=None #line,col of current function body open
func_body_close_line_col=None #line,col of current function body close

template_open = False  #inside a template argument list
template_trib_depth=None   #triangular brackets depth of template argument list open

cname="" #class name
class_open=False  #are we inside a class definition body
class_brace_depth=None #depth of braces of class definition open

paren_depth = 0   #how many ( ( (  deep are we?
brace_depth = 0   #how many { { {  deep are we?
trib_depth = 0 #how many < < < deep are we?

functions = [] #all functions found

for i in range(2, len(all_tokens)):
    tok = all_tokens[i]
    prev = all_tokens[i-1]
    prev2 = all_tokens[i-2]
    #print("Token ", tok.token_value, tok.token_type, " Status arg=", func_arg_open, "postarg=", func_postargs_open, "body=", func_body_open)

    #Track depths
    if tok.token_value == '(':
        paren_depth += 1
    elif tok.token_value == ')':
        paren_depth -= 1
    elif tok.token_value == '{':
        brace_depth += 1
    elif tok.token_value == '}':
        brace_depth -= 1
    elif tok.token_value == '<':
        trib_depth += 1
    elif tok.token_value == '>':
        trib_depth -= 1

        
    #Track preprocessor crud
    if tok.token_type == sctokenizer.TokenType.KEYWORD and prev.token_value == '#':
        in_preproc = True
        preproc_prev_line = tok.line
        print("Found preprocessor start: ",prev.token_value,tok.token_value,"on line",tok.line)
        
    if in_preproc == True and tok.line != preproc_prev_line:
        #Terminate the preproc iff no continuance on previous line
        if prev.token_value == "\\":
            preproc_prev_line += 1
            print("Found preprocessor continuance on line",tok.line)
        else:
            in_preproc = False
            print("Found preprocessor termination on line",tok.line)

    #Track templates
    if tok.token_type == sctokenizer.TokenType.KEYWORD and tok.token_value == 'template':
        template_open = True
        template_trib_depth = trib_depth  #hopefully nobody does anything evil like somehow use > as an operator inside a template parameter list!
    elif template_open == True and tok.token_value == '>' and trib_depth == template_trib_depth:
        template_open = False

            
    #Track classes and structs   
    if template_open == False and ( tok.token_value == "class" or tok.token_value == "struct" ):
        cname = all_tokens[i+1].token_value
        class_open = True
        class_brace_depth = brace_depth
        print("Found class/struct \"%s\" open on line %d" % (cname,tok.line))
    elif class_open == True and tok.token_value == '}' and brace_depth == class_brace_depth:
        print("Found class/struct \"%s\" close on line %d" % (cname,tok.line))
        class_open = False
            
    #Main function logic
    if func_arg_open == True:
        #Inside arguments for a function
        fargs += tok.token_value + " "
        
        #Need to look for the close parentheses
        if tok.token_value == ')' and paren_depth == func_paren_depth:
            func_arg_open = False
            func_postargs_open = True

            #Detect initializer lists
            if all_tokens[i+1].token_value == ':':
                func_postargs_initializer_list = True
            else:
                func_postargs_initializer_list = False

            #Detect auto return types
            if all_tokens[i+1].token_value == '->':                
                func_postargs_auto_type=True
            else:
                func_postargs_auto_type=False

    elif func_postargs_open == True:
        #Between ) and {
        #Terminate search on encountering a ';' as this cannot be a function definition
        if tok.token_value == ';':
            func_postargs_open = False

            
        #If it is not an initializer list or auto return type we should not encounter any identifier
        elif func_postargs_initializer_list == False and func_postargs_auto_type == False and tok.token_type == sctokenizer.TokenType.IDENTIFIER:
            print("Encountered unexpected identifier in function postargs, line", tok.line)
            func_postargs_open = False
        
            
        elif paren_depth == func_paren_depth and tok.token_value == '{':
            #The open { have to be at the same ( depth to exclude things like   MyClass(...): a({init-list}) {   from picking up the brace inside the parentheses

            #Another edge case is if this is in an initializer list with the newer syntax   eg   MyClass() :  anobject{...} {
            if func_postargs_initializer_list == True and prev.token_type == sctokenizer.TokenType.IDENTIFIER:
                print("Found initialization %s %s on line %d" % (prev.token_value, tok.token_value, tok.line))
                #pass
                
            else:            
                print("Found function start %s %s on line %d" % (fname, fargs, tok.line))
                func_postargs_open = False
                func_body_open = True
                func_body_brace_depth = brace_depth-1
                func_body_open_line_col = (tok.line,tok.column)
            
    elif func_body_open == True:
        #Look for the close braces that indicate the end of the function body
        if brace_depth == func_body_brace_depth and tok.token_value == '}':
            func_body_open = False
            func_body_close_line_col = (tok.line,tok.column)
            print("Found body open (%d,%d) and close (%d,%d)" % (func_body_open_line_col[0],func_body_open_line_col[1],func_body_close_line_col[0],func_body_close_line_col[1]))
            functions.append( Function(fname, fargs, func_body_open_line_col, func_body_close_line_col) )
                        
    elif in_preproc == False and tok.token_value == '(' and prev.token_type == sctokenizer.TokenType.IDENTIFIER and prev.token_value != 'for' and prev.token_value != 'while' and prev.token_value != 'if':
        #This looks like a function but not necessarily a definition
        print("Found possible function open %s %s" % (prev.token_value, tok.token_value))

        #Skip __global__, constexpr etc keywords
        found_keyword, keyword_idx = func_keyword_check(i-2, all_tokens, ['constexpr','global__','device__'])  #todo: rather than checking back for keywords we can record currently open keywords and flush the list on encountering a function body
        if found_keyword  == True:
            print("Found skipped keyword %s" % (all_tokens[keyword_idx].token_value))
        else:
            #Found a potential function
            fname=prev.token_value
            if class_open == True:
                #Inside class body
                fname = cname + '::' + fname
            elif prev2.token_value == '::':
                #Class method definition
                cname = rewind_class_name(i-1, all_tokens)
                if cname != None:
                    fname = cname + '::' + fname

            fargs=tok.token_value
            func_arg_open=True
            func_paren_depth = paren_depth-1  #depth outside of parentheses

f = open(filename,'r')
flines = f.readlines()
f.close()
line_offsets = {}

for func in functions:
    line = func.open_line_col[0]
    col = func.open_line_col[1]

    iline=line-1 #lines in the tokenizer start at 1
    icol=col-1 #so do columns
    
    #If we've previously modified this line we need to add an offset
    if line in line_offsets:
        icol += line_offsets[line]


    #fargs = func.args
    #Need to ensure string double quotes have backslash
    #fargs = re.sub(r'"',r'\"',fargs)
        
    #Form string to insert
    #toins = "TAU_PROFILE(\"%s\", \"%s\", TAU_DEFAULT);" % (func.name, fargs)

    #Use perfstubs, which automatically collects the function info
    toins = "PERFSTUBS_SCOPED_TIMER_FUNC();"
    if func.name == "main":
        toins = "PERFSTUBS_INITIALIZE(); " + toins
    
    orig = flines[iline]   
    #print("Line %d char %d is %s,  expect {" % (iline,icol,orig[icol]))

    
    flines[iline] = orig[:icol+1] + toins + orig[icol+1:]
    print("Modified line %d : from\n\"%s\"\nto\n\"%s\"" % (line, orig, flines[iline])  )

    #Set the new line offset
    if line in line_offsets:
        line_offsets[line] += len(toins)
    else:
        line_offsets[line] = len(toins)


f = open(filename + '.inst', 'w')
f.write('#include "perfstubs_api/timer.h"\n')
f.write('#ifdef __HIP_DEVICE_COMPILE__\n#undef PERFSTUBS_SCOPED_TIMER_FUNC\n#define PERFSTUBS_SCOPED_TIMER_FUNC()\n#endif\n')
for line in flines:
    f.write(line)
f.close()

