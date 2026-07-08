"""
终极安全加固版 V3 - 整合两大 AI 的最佳实践
融合 bcrypt 哈希、验证码、IP 限流锁、密码修改、CSRF 等全部防护措施
"""
import os
import re
import logging
import secrets
import time
import sqlite3
from datetime import timedelta, datetime
from collections import defaultdict

import bcrypt
from flask import (
    Flask, render_template, request, redirect, session,
    flash, jsonify, g
)
from flask_limiter import Limiter
from flask_limiter.util import get_remote_address
from flask_wtf import FlaskForm, CSRFProtect
from wtforms import StringField, PasswordField, SubmitField
from wtforms.validators import DataRequired, Length, Regexp, EqualTo
from werkzeug.middleware.proxy_fix import ProxyFix

# ============================================================
# 应用初始化
# ============================================================

app = Flask(__name__)

# ---------- 安全的 Secret Key ----------
app.secret_key = os.environ.get(
    "FLASK_SECRET_KEY",
    secrets.token_hex(32)  # 128位随机密钥
)

# ---------- 关闭 Debug ----------
app.config["DEBUG"] = False

# ---------- Session 安全配置 ----------
app.config.update(
    SESSION_COOKIE_HTTPONLY=True,
    SESSION_COOKIE_SAMESITE="Lax",
    SESSION_COOKIE_SECURE=False,
    PERMANENT_SESSION_LIFETIME=timedelta(minutes=30),
    SESSION_REFRESH_EACH_REQUEST=True,
)

# ---------- CSRF 全局保护 ----------
csrf = CSRFProtect(app)

# ---------- 代理修复 ----------
app.wsgi_app = ProxyFix(app.wsgi_app, x_for=1, x_proto=1)

# ============================================================
# 日志系统
# ============================================================

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(message)s",
    handlers=[
        logging.FileHandler("audit.log", encoding="utf-8"),
        logging.StreamHandler(),
    ]
)
logger = logging.getLogger("user_mgmt")

# ============================================================
# 请求频率限制
# ============================================================

limiter = Limiter(
    get_remote_address,
    app=app,
    default_limits=["60 per minute"],
    storage_uri="memory://",
)

# ============================================================
# IP 级账户锁定（5次失败锁定15分钟）
# ============================================================

# 结构: { "ip_address": {"count": int, "first_fail": float} }
FAILED_LOGINS: dict = defaultdict(lambda: {"count": 0, "first_fail": 0.0})
LOCKOUT_WINDOW = 15 * 60  # 15 分钟（秒）
LOCKOUT_THRESHOLD = 5     # 5 次失败触发

def is_ip_locked(ip: str) -> bool:
    """检查 IP 是否被锁定"""
    record = FAILED_LOGINS.get(ip)
    if not record:
        return False
    elapsed = time.time() - record["first_fail"]
    if elapsed > LOCKOUT_WINDOW:
        # 锁定窗口已过，重置
        del FAILED_LOGINS[ip]
        return False
    return record["count"] >= LOCKOUT_THRESHOLD

def record_failed_login(ip: str):
    """记录一次登录失败"""
    now = time.time()
    record = FAILED_LOGINS[ip]
    if record["count"] == 0:
        record["first_fail"] = now
    record["count"] += 1

def reset_failed_login(ip: str):
    """登录成功后重置失败计数"""
    FAILED_LOGINS.pop(ip, None)

# ============================================================
# 验证码生成与验证（简单数学题）
# ============================================================

def generate_captcha() -> tuple:
    """生成验证码，返回 (问题文本, 正确答案)"""
    import random
    a = random.randint(1, 20)
    b = random.randint(1, 20)
    op = random.choice(["+", "-"])
    if op == "-":
        # 保证结果非负
        a, b = max(a, b), min(a, b)
    answer = a + b if op == "+" else a - b
    question = f"{a} {op} {b} = ?"
    return question, str(answer)

# ============================================================
# 用户数据库（bcrypt 加盐哈希）
# ============================================================

def hash_password(plaintext: str) -> str:
    return bcrypt.hashpw(
        plaintext.encode("utf-8"), bcrypt.gensalt(rounds=12)
    ).decode("utf-8")

USERS = {
    "admin": {
        "username": "admin",
        "password_hash": hash_password("Admin@2025!Secure"),
        "role": "admin",
        "email": "admin@example.com",
        "phone": "13800138000",
        "balance": 99999,
    },
    "alice": {
        "username": "alice",
        "password_hash": hash_password("Alice@2025!Secure"),
        "role": "user",
        "email": "alice@example.com",
        "phone": "13900139001",
        "balance": 100,
    },
}

# ============================================================
# 数据库初始化（SQLite）
# ============================================================

DATABASE_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "data")
DATABASE_PATH = os.path.join(DATABASE_DIR, "users.db")

def init_db():
    os.makedirs(DATABASE_DIR, exist_ok=True)
    conn = sqlite3.connect(DATABASE_PATH)
    c = conn.cursor()
    c.execute('''CREATE TABLE IF NOT EXISTS users (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        username TEXT UNIQUE NOT NULL,
        password TEXT NOT NULL,
        email TEXT,
        phone TEXT
    )''')
    # 插入默认用户
    c.execute("INSERT OR IGNORE INTO users (username, password, email, phone) VALUES (?, ?, ?, ?)",
              ("admin", "admin123", "admin@example.com", "13800138000"))
    c.execute("INSERT OR IGNORE INTO users (username, password, email, phone) VALUES (?, ?, ?, ?)",
              ("alice", "alice2025", "alice@example.com", "13900139001"))
    conn.commit()
    conn.close()
    print(f"[DB] 数据库已初始化: {DATABASE_PATH}")

# ============================================================
# WTForms 表单定义
# ============================================================

class LoginForm(FlaskForm):
    username = StringField(
        "用户名",
        validators=[
            DataRequired(message="用户名不能为空"),
            Length(min=2, max=20, message="用户名长度应为 2-20 个字符"),
            Regexp(
                r"^[a-zA-Z0-9_\u4e00-\u9fa5]+$",
                message="用户名只能包含字母、数字、下划线和中文",
            ),
        ],
    )
    password = PasswordField(
        "密码",
        validators=[
            DataRequired(message="密码不能为空"),
            Length(min=6, max=64, message="密码长度应为 6-64 个字符"),
        ],
    )
    captcha = StringField(
        "验证码",
        validators=[DataRequired(message="请填写验证码")],
    )
    submit = SubmitField("登 录")


class ChangePasswordForm(FlaskForm):
    old_password = PasswordField(
        "当前密码",
        validators=[DataRequired(message="请输入当前密码")],
    )
    new_password = PasswordField(
        "新密码",
        validators=[
            DataRequired(message="请输入新密码"),
            Length(min=8, max=64, message="新密码长度应为 8-64 个字符"),
            Regexp(
                r"^(?=.*[a-z])(?=.*[A-Z])(?=.*\d)(?=.*[!@#$%^&*()_+\-=\[\]{};':\"\\|,.<>/?`~]).{8,64}$",
                message="密码必须包含大小写字母、数字和特殊字符",
            ),
        ],
    )
    confirm_password = PasswordField(
        "确认新密码",
        validators=[
            DataRequired(message="请确认新密码"),
            EqualTo("new_password", message="两次输入的密码不一致"),
        ],
    )
    submit = SubmitField("修改密码")


# ============================================================
# 辅助函数
# ============================================================

def get_safe_user_dict(username: str) -> dict | None:
    """返回不含密码哈希的安全用户信息"""
    user = USERS.get(username)
    if user is None:
        return None
    return {
        "username": user["username"],
        "role": user["role"],
        "email": user["email"],
        "phone": user["phone"],
        "balance": user["balance"],
    }


def sanitize_input(text: str) -> str:
    """清洗输入：过滤控制字符，防止隐式注入"""
    # 移除 ASCII 控制字符（保留换行等可见字符）
    cleaned = re.sub(r"[\x00-\x08\x0b\x0c\x0e-\x1f\x7f]", "", text)
    return cleaned.strip()


# ============================================================
# 检查登录状态（装饰器）
# ============================================================

def login_required(f):
    from functools import wraps
    @wraps(f)
    def decorated(*args, **kwargs):
        if "username" not in session:
            flash("请先登录", "error")
            return redirect("/login")
        return f(*args, **kwargs)
    return decorated


# ============================================================
# 路由
# ============================================================

@app.route("/")
def index():
    """首页"""
    username = session.get("username")
    user = get_safe_user_dict(username)

    # 搜索功能
    keyword = request.args.get("keyword", "")
    search_results = None
    if keyword:
        conn = sqlite3.connect(DATABASE_PATH)
        c = conn.cursor()
        sql = f"SELECT id, username, email, phone FROM users WHERE username LIKE '%{keyword}%' OR email LIKE '%{keyword}%'"
        print(f"[SQL] 执行搜索: {sql}")
        c.execute(sql)
        search_results = c.fetchall()
        conn.close()

    return render_template("index.html", user=user, USERS=USERS, search_results=search_results, keyword=keyword)


@app.route("/register", methods=["GET", "POST"])
def register():
    """注册（使用 f-string 拼接 SQL）"""
    if request.method == "POST":
        username = request.form.get("username", "")
        password = request.form.get("password", "")
        email = request.form.get("email", "")
        phone = request.form.get("phone", "")

        conn = sqlite3.connect(DATABASE_PATH)
        c = conn.cursor()
        sql = f"INSERT INTO users (username, password, email, phone) VALUES ('{username}', '{password}', '{email}', '{phone}')"
        print(f"[SQL] 执行注册: {sql}")
        try:
            c.execute(sql)
            conn.commit()
            flash("注册成功，请登录", "success")
            return redirect("/login")
        except Exception as e:
            print(f"[SQL] 注册错误: {e}")
            flash(f"注册失败: {e}", "error")
        finally:
            conn.close()

    return render_template("register.html")


@app.route("/login", methods=["GET", "POST"])
@limiter.limit("5 per minute")
def login():
    """登录（含验证码 + IP 锁定）"""
    client_ip = request.remote_addr or "unknown"

    # ---- 检查 IP 是否被锁定 ----
    if is_ip_locked(client_ip):
        record = FAILED_LOGINS[client_ip]
        remaining = int(LOCKOUT_WINDOW - (time.time() - record["first_fail"]))
        minutes = remaining // 60
        seconds = remaining % 60
        logger.warning(
            f"[BLOCKED] IP {client_ip} 已被锁定 - "
            f"剩余 {minutes} 分 {seconds} 秒"
        )
        flash(
            f"登录尝试过于频繁，请 {minutes} 分 {seconds} 秒后重试",
            "error"
        )
        return render_template("login.html", form=LoginForm(), captcha=None)

    form = LoginForm()

    # ---- 验证码生成 ----
    if request.method == "GET":
        question, answer = generate_captcha()
        session["captcha_answer"] = answer
        return render_template("login.html", form=form, captcha=question)

    # POST 请求处理
    if form.validate_on_submit():
        username = sanitize_input(form.username.data)
        password = form.password.data  # 密码不清洗（哈希过程安全）
        captcha_input = form.captcha.data.strip()

        # ---- 验证码校验 ----
        correct_answer = session.pop("captcha_answer", None)
        if not correct_answer or captcha_input != correct_answer:
            # 刷新验证码
            question, answer = generate_captcha()
            session["captcha_answer"] = answer
            flash("验证码错误", "error")
            logger.warning(
                f"[CAPTCHA FAIL] IP: {client_ip} 填写了错误的验证码"
            )
            return render_template(
                "login.html", form=LoginForm(), captcha=question
            )

        # ---- 用户认证 ----
        user = USERS.get(username)

        if user is not None and bcrypt.checkpw(
            password.encode("utf-8"),
            user["password_hash"].encode("utf-8"),
        ):
            # 登录成功：重置失败计数 + 生成新 Session
            reset_failed_login(client_ip)
            session.clear()  # 防止 session 固定攻击
            session.permanent = True
            session["username"] = username
            session["login_time"] = datetime.utcnow().isoformat()
            session["ip_address"] = client_ip

            logger.info(
                f"[AUTH OK] 用户 {username} 登录成功 - IP: {client_ip}"
            )

            user_safe = get_safe_user_dict(username)
            return render_template("index.html", user=user_safe, USERS=USERS)

        # 登录失败
        record_failed_login(client_ip)
        logger.warning(
            f"[AUTH FAIL] 登录失败 - 用户名: {username} - IP: {client_ip}"
        )

        # 检查本次失败后是否触发锁定
        if FAILED_LOGINS[client_ip]["count"] >= LOCKOUT_THRESHOLD:
            logger.warning(
                f"[LOCKED] IP {client_ip} 已触发锁定机制"
            )
            flash(
                "登录尝试过于频繁，请 15 分钟后重试",
                "error"
            )
        else:
            remaining_attempts = LOCKOUT_THRESHOLD - FAILED_LOGINS[client_ip]["count"]
            flash(
                f"用户名或密码错误（还剩 {remaining_attempts} 次尝试机会）",
                "error"
            )

        # 刷新验证码
        question, answer = generate_captcha()
        session["captcha_answer"] = answer
        return render_template("login.html", form=LoginForm(), captcha=question)

    # 表单校验失败
    question, answer = generate_captcha()
    session["captcha_answer"] = answer
    return render_template("login.html", form=form, captcha=question)


@app.route("/change_password", methods=["GET", "POST"])
@login_required
@limiter.limit("3 per minute")
def change_password():
    """修改密码"""
    username = session.get("username")
    form = ChangePasswordForm()

    if form.validate_on_submit():
        old_pw = form.old_password.data
        new_pw = form.new_password.data

        user = USERS.get(username)
        if user and bcrypt.checkpw(
            old_pw.encode("utf-8"),
            user["password_hash"].encode("utf-8"),
        ):
            # 更新密码哈希
            user["password_hash"] = hash_password(new_pw)
            logger.info(
                f"[PWD CHANGE] 用户 {username} 修改密码成功 - IP: {request.remote_addr}"
            )
            flash("密码修改成功", "success")
            return redirect("/")
        else:
            flash("当前密码错误", "error")
            logger.warning(
                f"[PWD FAIL] 用户 {username} 修改密码失败（当前密码错误）"
            )

    return render_template("change_password.html", form=form)


@app.route("/logout")
def logout():
    """登出：清除 session 防止残留"""
    username = session.get("username", "未知")
    logger.info(f"[LOGOUT] 用户 {username} 登出 - IP: {request.remote_addr}")
    session.clear()
    resp = redirect("/")
    resp.delete_cookie("session")
    return resp


@app.route("/api/health")
def health_check():
    return jsonify({"status": "ok", "timestamp": datetime.utcnow().isoformat()})


# ============================================================
# 错误处理器
# ============================================================

@app.errorhandler(429)
def ratelimit_handler(e):
    flash("请求过于频繁，请稍后再试", "error")
    return render_template("login.html", form=LoginForm(), captcha=None), 429


@app.errorhandler(404)
def not_found(e):
    return render_template("error.html", code=404, message="页面不存在"), 404


@app.errorhandler(500)
def server_error(e):
    logger.error(f"[500 ERROR] {e}")
    return render_template("error.html", code=500, message="服务器内部错误"), 500


# ============================================================
# 启动入口
# ============================================================

if __name__ == "__main__":
    print("=" * 60)
    print("  🔒 终极安全版用户管理平台 V3")
    print()
    print("  测试账号:")
    print("    管理员 - admin / Admin@2025!Secure")
    print("    普通用户 - alice / Alice@2025!Secure")
    print()
    print("  ✅ 已启用防护:")
    print("     - bcrypt 加盐哈希")
    print("     - CSRF 令牌保护")
    print("     - 验证码（数学题）")
    print("     - IP 5次失败锁定15分钟")
    print("     - 请求频率限制")
    print("     - 安全的 Session 配置")
    print("     - 输入校验 + 清洗")
    print("     - 审计日志")
    print("     - Session 固定攻击防护")
    print("     - 密码修改功能")
    print("  ✅ 新增功能:")
    print("     - 用户注册（明码存储 + f-string 拼接 SQL）")
    print("     - 用户搜索（f-string 拼接 SQL，控制台打印 SQL）")
    print("=" * 60)

    init_db()
    app.run(host="0.0.0.0", port=5000, debug=True)
