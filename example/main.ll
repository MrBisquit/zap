; ModuleID = 'zap_module'
source_filename = "zap_module"

@0 = private unnamed_addr constant [13 x i8] c"hello world\0A\00", align 1

declare i32 @puts(ptr)

define internal void @println(ptr %0) {
entry:
  %1 = call i32 @puts(ptr %0)
  ret void
}

define i32 @main() {
entry:
  call void @println(ptr @0)
  ret i32 0
}
