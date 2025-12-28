; ModuleID = 'zap_module'
source_filename = "zap_module"

define i32 @main() {
entry:
  %a = alloca i32, align 4
  store i32 25, ptr %a, align 4
  %a1 = load i32, ptr %a, align 4
  %0 = add i32 %a1, 1
  ret i32 %0
}
