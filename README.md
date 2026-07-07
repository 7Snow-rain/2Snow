# 用户管理系统 V3（终极安全加固版）

> 一个安全加固的 Flask 用户管理应用，整合了 13 项安全防护措施。
> 用于网络安全课程作业，展示常见 Web 漏洞及修复方案。

## 🔒 已实现的安全防护

| 防护措施 | 说明 |
|----------|------|
| bcrypt 加盐哈希 | 密码不以明文存储，抗彩虹表攻击 |
| 验证码（数学题） | 防止自动化登录攻击 |
| IP 5次失败锁定15分钟 | 爆破自动封禁 |
| Flask-Limiter 频率限制 | 登录接口 5次/分钟 |
| CSRF 令牌保护 | Flask-WTF 全局防护 |
| Session 安全配置 | HttpOnly + SameSite + 30分钟过期 |
| Session 固定攻击防护 | 登录后重新生成 Session |
| 输入校验 | WTForms + 正则白名单 + 控制字符清洗 |
| Jnja2 自动转义 | 防止 XSS 注入 |
| 审计日志 | 记录所有安全事件及 IP |
| 密码修改功能 | 含复杂度校验 |

## 🚀 快速启动

```bash
pip install -r requirements.txt
python3 app.py
```

访问 `http://localhost:5000`

## 📝 测试账号

| 用户名 | 密码 | 角色 |
|--------|------|------|
| admin | Admin@2025!Secure | 管理员 |
| alice | Alice@2025!Secure | 普通用户 |

## 📄 文档

- `SECURITY_REPORT.md` — 完整的漏洞修复报告（作业可直接使用）
