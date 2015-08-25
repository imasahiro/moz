#!ruby
TAB="    "
$line = 0

file = ARGV[0]
vmcore = ARGV[1]
vminst = ARGV[2]

types = []

open(file) {|f|
  out = open(vmcore, "w")
  while l = f.gets
    if l =~ /^DEF\((.*)\)/
      a = $1.split(",")
      op = a.shift
      # puts "#line #{$line} \"#{FILE}\""
      out.puts "OP_CASE(#{op})\n{"
      types << [op, *a.map{|e| e.split(" ")[0].strip }]
      a.each {|e|
        e = e.strip
        type, name = e.split(" ")
        out.puts TAB + e + " = read_#{type}(PC);"
      }
    elsif l == "{\n"
    elsif l == "}\n"
      out.puts TAB + "NEXT();"
      out.puts "}"
    else
      out.puts l
    end
    $line += 1
  end

  out = open(vminst, "w")
  out.puts <<-TXT
#ifdef MOZVM_OPCODE_SIZE
static unsigned opcode_size(int opcode)
{
    switch (opcode) {
TXT
  types.each {|e|
    op = e[0]
    size = ["MOZVM_INST_HEADER_SIZE", *(e[1..-1].map{|e| "sizeof(#{e})" })].join(" + ")
    out.puts TAB + "case #{op}: return #{size};"
  }
  out.puts <<-TXT
    default: break;
    }
    return -1;
}
#endif /*MOZVM_OPCODE_SIZE*/
TXT
}
