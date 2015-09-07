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
      types << [op, *a]
      a.each {|e|
        e = e.strip
        type, name, cond = e.split(" ")
        if cond != nil
          out.puts "#ifdef " + cond
        end
        out.puts TAB + "#{type} #{name} = read_#{type}(PC);"
        if cond != nil
          out.puts "#endif /* #{cond} */"
        end

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
#ifndef #{vminst.upcase.gsub(".", "_").gsub("/", "_")}
#define #{vminst.upcase.gsub(".", "_").gsub("/", "_")}

#ifdef MOZVM_OPCODE_SIZE
static unsigned opcode_size(int opcode)
{
    unsigned size = MOZVM_INST_HEADER_SIZE;
    switch (opcode) {
TXT
  tab2 = TAB + TAB
  types.each {|a|
    op = a.shift
    out.puts TAB + "case #{op}:"
    a.each do |e|
      type, name, cond = e.split(" ")
      if cond != nil
        out.puts "#ifdef " + cond
      end
      out.puts tab2 + "size += sizeof(#{type.strip});"
      if cond != nil
        out.puts "#endif /* #{cond} */"
      end
    end
    out.puts tab2 + "return size;"
  }
  out.puts <<-TXT
    default: break;
    }
    return -1;
}
#endif /*MOZVM_OPCODE_SIZE*/
#endif /* end of include guard */
TXT
}
