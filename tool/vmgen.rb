#!ruby
TAB="    "
$line = 0

file = ARGV[0]
vmcore = ARGV[1]
vminst = ARGV[2]
prefix = ARGV[3]

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
#ifndef #{vminst.upcase.gsub(".", "_").gsub("/", "_").gsub("-", "_")}
#define #{vminst.upcase.gsub(".", "_").gsub("/", "_").gsub("-", "_")}
TXT

  out.puts <<-TXT
#ifdef MOZVM_#{prefix.upcase}_OPCODE_SIZE
static unsigned #{prefix}_opcode_size(int opcode)
{
    unsigned size = MOZVM_INST_HEADER_SIZE;
    switch (opcode) {
TXT
  tab2 = TAB + TAB
  types.each {|a|
    a = a.dup
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
#endif /*MOZVM_#{prefix.upcase}_OPCODE_SIZE */
TXT

  out.puts <<-TXT
#ifdef MOZVM_#{prefix.upcase}_DUMP
static moz_inst_t *#{prefix}_dump(FILE *out, moz_runtime_t *R, moz_inst_t *inst)
{
    uint8_t opcode = *(uint8_t *)inst;
    inst++;
    switch (opcode) {
TXT
  tab2 = TAB + TAB
  types.each {|a|
    a = a.dup
    op = a.shift
    out.puts TAB + "case #{op}:"
    out.puts tab2 + "dump_opcode(out, R, opcode);"
    a.each do |e|
      type, name, cond = e.split(" ")
      if cond != nil
        out.puts "#ifdef " + cond
      end
      out.puts tab2 + "dump_#{type}(out, R, *(#{type} *) inst);"
      out.puts tab2 + "inst += sizeof(#{type});"
      if cond != nil
        out.puts "#endif /* #{cond} */"
      end
    end
    out.puts tab2 + "return inst;"
  }
  out.puts <<-TXT
    default: break;
    }
    return NULL;
}
#endif /*MOZVM_#{prefix.upcase}_DUMP */
TXT
  out.puts <<-TXT
#endif /* end of include guard */
TXT
}
