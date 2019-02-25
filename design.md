## 总体设计
### 组件
- Client
- Broker
- Judger
- Compiler
- Tester
- FileAgent
- 各种Checker、RandomGenerator等

### 外部服务
- FileService（暂定seaweedfs）
- Mysql（评测配置、结果）
- Redis（评测状态）

### 前置条件
- File Service已存在题目相关文件（problems/[problem_id]/）：
  - in,out
  - solution
  - random
- File Service已存在提交的代码文件（submission/[sub_id]/）：
  - [filename]

### 评测流程
1. Client向Judger提供sub_id,problem_id
2. Judger保存评测状态（Redis）
  2.1. 调用Compiler，Compiler把a.out存入FileAgent目录
  2.2. 调用Tester，Tester执行a.out并比对，返回结果
  可选：静态检查、内存检查、随机测试
3. Judger向Client返回评测结果

### 可靠的消息传递
- Client重试
- 服务组件的心跳
- （代理主从）

### 优势
- 异构：各组件可用不同的技术栈开发
- 可扩展：Compiler、Tester等组件可以（自动or手动）部署合适的数量
- 健壮：部分组件的局部崩溃可以自动修复

## 局部设计
### Client
- RESTful API
- Nodejs

### Judger
- C++