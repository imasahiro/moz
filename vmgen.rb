$typemap = {}
vm = []
ops = []

$vmfile = open("vm.c", "w")
$bcfile = open("MozInstruction.java", "w")

class Op
  attr_accessor :id, :name, :args, :desc
  def initialize(id, name, args, desc)
    @id = id
    @name = name
    @args = args
    @desc = desc
  end

  def print_struct(f)
    f.puts <<-TEXT
/* #{@name} :: #{@desc} */
#define OPCODE_#{@name} #{@id}
struct I#{@name} {
    uint8_t opcode;
    TEXT
    @args.split(" ").each_with_index {|e, i|
      f.puts "    #{$typemap[e]} op#{i};"
    }
    f.puts "};\n\n"
  end
end

open("spec.txt") {|f|
  while l = f.gets
    # argument type
    if l =~ /^\+/
      (type1, type2) = l.split("|").map {|e| e.strip }
      type1.gsub!("+", "")
      $typemap[type1] = type2
    end

    # instruction
    if l =~ /^\*/
      (op, args, desc) = l.split("|").map {|e| e.strip }
      inst = Op.new(ops.size, op.gsub!("*",""), args, desc)
      ops << inst
    end
  end
}

ops.each {|inst|
  inst.print_struct($vmfile)
}

$vmfile.puts "#define OP_EACH(OP) \\"
ops.each {|inst|
  $vmfile.puts "    OP(#{inst.name})\\"
}

$bcfile.puts <<-TEXT
package nez.generator.moz;

enum MozOpcode {
TEXT
ops.each {|inst|
  $bcfile.puts "    M#{inst.name},"
}
$bcfile.puts <<-TEXT
}

class MozInstruction {
    BasicBlock parent = null;
    MozOpcode opcode;
    public MozInstruction(MozOpcode opcode) {
        this.opcode = opcode;
    }
}
TEXT

ops.each {|inst|
  $bcfile.puts <<-TEXT
class Moz#{inst.name} extends MozInstruction {
    public Moz#{inst.name}() {
      super(MozOpcode.M#{inst.name});
    }

    @Override
    public String toString() {
      return "(M#{inst.name})";
    }
}
  TEXT
}
