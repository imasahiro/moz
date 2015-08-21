#!ruby
TAB="    "
$line = 0
FILE="src/instruction.def"

types = []
open(FILE) {|f|
  while l = f.gets
    if l =~ /DEF\((.*)\)/
      a = $1.split(",")
      op = a.shift
      # puts "#line #{$line} \"#{FILE}\""
      puts "OP_CASE(#{op}) {"
      types << [op, *a.map{|e| e.split(" ")[0].strip }]
      a.each {|e|
        type, name = e.strip.split(" ")
        puts TAB + e + " = read_#{type}(PC);"
      }
    elsif l == "{\n"
    elsif l == "}\n"
      puts TAB + "NEXT();"
      puts "}"
    else
      puts l
    end
    $line += 1
  end
}

f = open("src/vm_inst.h", "w")
f.puts <<-TXT
#ifdef MOZVM_OPCODE_SIZE
static unsigned opcode_size(int opcode)
{
    switch (opcode) {
TXT
types.each {|e|
  op = e[0]
  size = [1, *(e[1..-1].map{|e| "sizeof(#{e})" })].join(" + ")
  f.puts TAB + "case #{op}: return #{size};"
}
f.puts <<-TXT
    default: break;
    }
    return -1;
}
#endif /*MOZVM_OPCODE_SIZE*/
TXT
