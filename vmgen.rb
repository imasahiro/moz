#!ruby
TAB="    "
$line = 0
FILE="src/instruction.def"
open(FILE) {|f|
  while l = f.gets
    if l =~ /DEF\((.*)\)/
      a = $1.split(",")

      puts "#line #{$line} \"#{FILE}\""
      puts "OP_CASE(#{a.shift}) {"
      a.each {|e|
        puts TAB + e + ";"
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
