1.lamba optional

## 第三部分完成后应该真正学会什么

完成这一阶段后，你应该能够：

* 讲清楚一次 `SET/GET` 从 TCP 字节到数据库对象再到 RESP 响应的完整调用链。
* 设计协议层、命令层和存储层之间的稳定边界。
* 正确处理 Redis 命令的类型、缺失值、边界和错误语义。
* 解释渐进式 rehash 如何用额外状态换取更平滑的尾延迟。
* 解释 ZSet 为什么需要 HashTable 和 SkipList 两个索引。
* 使用 invariant、随机测试和 Redis oracle 验证数据结构，而不只是依赖几个手写样例。
