"""World / Characters veritabani islemleri (waypoint_data, creature, creature_addon)."""
import pymysql

WP_COLS = ["point", "position_x", "position_y", "position_z", "orientation",
           "delay", "move_type", "action", "action_chance"]


class Database:
    def __init__(self, cfg: dict):
        self.cfg = cfg
        self.world = None
        self.chars = None

    def _conn(self, dbname):
        return pymysql.connect(
            host=self.cfg.get("host", "127.0.0.1"), port=int(self.cfg.get("port", 3306)),
            user=self.cfg.get("user", "root"), password=self.cfg.get("password", ""),
            database=dbname, charset="utf8mb4", autocommit=True, connect_timeout=8,
            cursorclass=pymysql.cursors.DictCursor,
        )

    def connect(self):
        self.close()
        self.world = self._conn(self.cfg.get("world_db", "world"))
        if self.cfg.get("characters_db"):
            self.chars = self._conn(self.cfg["characters_db"])

    def close(self):
        for c in (self.world, self.chars):
            try:
                if c:
                    c.close()
            except Exception:
                pass
        self.world = self.chars = None

    def _w(self):
        if not self.world:
            raise RuntimeError("World veritabanina bagli degil (Ayarlar sekmesi).")
        self.world.ping(reconnect=True)
        return self.world.cursor()

    # ---------- waypoint_data ----------
    def get_path(self, path_id: int):
        with self._w() as c:
            c.execute(f"SELECT {', '.join(WP_COLS)} FROM waypoint_data WHERE id=%s ORDER BY point", (path_id,))
            return c.fetchall()

    def save_path(self, path_id: int, points: list):
        """Path'i tamamen yeniden yazar, noktalari 1..n olarak numaralar."""
        rows = [(path_id, i, p["position_x"], p["position_y"], p["position_z"], p["orientation"],
                 p["delay"], p["move_type"], p["action"], p["action_chance"])
                for i, p in enumerate(points, 1)]
        with self._w() as c:
            c.execute("DELETE FROM waypoint_data WHERE id=%s", (path_id,))
            if rows:
                c.executemany(
                    "INSERT INTO waypoint_data (id, point, position_x, position_y, position_z, orientation, "
                    "delay, move_type, action, action_chance) VALUES (%s,%s,%s,%s,%s,%s,%s,%s,%s,%s)", rows)

    def delete_path(self, path_id: int):
        with self._w() as c:
            c.execute("DELETE FROM waypoint_data WHERE id=%s", (path_id,))

    def list_paths(self, like: str = "", limit: int = 300):
        with self._w() as c:
            if like:
                c.execute("SELECT id, COUNT(*) AS points FROM waypoint_data WHERE CAST(id AS CHAR) LIKE %s "
                          "GROUP BY id ORDER BY id LIMIT %s", (like + "%", limit))
            else:
                c.execute("SELECT id, COUNT(*) AS points FROM waypoint_data GROUP BY id ORDER BY id DESC LIMIT %s",
                          (limit,))
            return c.fetchall()

    def next_free_path_id(self, guid: int = 0):
        """TrinityCore gelenegi: path_id = guid * 10. Doluysa +1 dener; guid yoksa MAX(id)+1."""
        with self._w() as c:
            if guid:
                pid = guid * 10
                while True:
                    c.execute("SELECT 1 FROM waypoint_data WHERE id=%s LIMIT 1", (pid,))
                    if not c.fetchone():
                        return pid
                    pid += 1
            c.execute("SELECT COALESCE(MAX(id), 0) + 1 AS n FROM waypoint_data")
            return int(c.fetchone()["n"])

    # ---------- creature ----------
    def find_creature(self, guid: int):
        with self._w() as c:
            c.execute(
                "SELECT c.guid, c.id AS entry, ct.name, c.map, c.position_x, c.position_y, c.position_z, "
                "c.orientation, c.MovementType, c.spawndist, CAST(COALESCE(ca.path_id, 0) AS UNSIGNED) AS path_id "
                "FROM creature c LEFT JOIN creature_template ct ON ct.entry = c.id "
                "LEFT JOIN creature_addon ca ON ca.guid = c.guid WHERE c.guid=%s", (guid,))
            return c.fetchone()

    def search_creatures(self, text: str, limit: int = 300):
        with self._w() as c:
            base = ("SELECT c.guid, c.id AS entry, ct.name, c.map, ROUND(c.position_x,2) AS x, "
                    "ROUND(c.position_y,2) AS y, ROUND(c.position_z,2) AS z, c.MovementType, "
                    "CAST(COALESCE(ca.path_id,0) AS UNSIGNED) AS path_id FROM creature c "
                    "LEFT JOIN creature_template ct ON ct.entry=c.id "
                    "LEFT JOIN creature_addon ca ON ca.guid=c.guid ")
            if text.isdigit():
                c.execute(base + "WHERE c.id=%s OR c.guid=%s LIMIT %s", (int(text), int(text), limit))
            else:
                c.execute(base + "WHERE ct.name LIKE %s LIMIT %s", (f"%{text}%", limit))
            return c.fetchall()

    def assign_path(self, guid: int, path_id: int, move_to_first=None):
        with self._w() as c:
            c.execute("INSERT INTO creature_addon (guid, path_id) VALUES (%s,%s) "
                      "ON DUPLICATE KEY UPDATE path_id=VALUES(path_id)", (guid, path_id))
            c.execute("UPDATE creature SET MovementType=2, spawndist=0 WHERE guid=%s", (guid,))
            if move_to_first:
                x, y, z, o = move_to_first
                c.execute("UPDATE creature SET position_x=%s, position_y=%s, position_z=%s, orientation=%s "
                          "WHERE guid=%s", (x, y, z, o, guid))

    def unassign_path(self, guid: int):
        with self._w() as c:
            c.execute("UPDATE creature_addon SET path_id=0 WHERE guid=%s", (guid,))
            c.execute("UPDATE creature SET MovementType=0 WHERE guid=%s", (guid,))

    # ---------- characters ----------
    def character_position(self, name: str):
        if not self.chars:
            raise RuntimeError("Characters veritabani ayarlanmamis (Ayarlar sekmesi).")
        self.chars.ping(reconnect=True)
        with self.chars.cursor() as c:
            c.execute("SELECT name, map, position_x, position_y, position_z, orientation, online "
                      "FROM characters WHERE name=%s", (name,))
            return c.fetchone()

    # ---------- serbest SQL ----------
    def raw(self, sql: str, target: str = "world"):
        conn = self.chars if target == "characters" else self.world
        if not conn:
            raise RuntimeError(f"{target} veritabanina bagli degil.")
        conn.ping(reconnect=True)
        with conn.cursor() as c:
            n = c.execute(sql)
            if c.description:
                return [d[0] for d in c.description], c.fetchall()
            return None, n


def path_to_sql(path_id: int, points: list) -> str:
    lines = [f"-- Path {path_id}", f"DELETE FROM `waypoint_data` WHERE `id`={path_id};"]
    if points:
        lines.append("INSERT INTO `waypoint_data` (`id`,`point`,`position_x`,`position_y`,`position_z`,"
                     "`orientation`,`delay`,`move_type`,`action`,`action_chance`) VALUES")
        vals = [f"({path_id},{i},{p['position_x']:.4f},{p['position_y']:.4f},{p['position_z']:.4f},"
                f"{p['orientation']:.4f},{p['delay']},{p['move_type']},{p['action']},{p['action_chance']})"
                for i, p in enumerate(points, 1)]
        lines.append(",\n".join(vals) + ";")
    return "\n".join(lines) + "\n"
