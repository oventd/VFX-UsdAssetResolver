from pymongo import MongoClient

# MongoDB 연결
client = MongoClient("mongodb://localhost:27017/")
db = client["WALL"]
collection = db["WALL"]

def get_usd_path(query_path: str) -> str:
    """
    query_path 예시: "AAA_084_0010/cache_usd_geo/v002"
    반환값: 해당 경로의 geo.usda 파일 경로 문자열
    """
    try:
        shot_name, cache_type, version = query_path.split('/')
    except ValueError:
        raise ValueError("Query path must be in 'shot/cache_type/version' format")

    doc = collection.find_one({"shot_name": shot_name})
    if not doc:
        raise ValueError(f"No shot found with name: {shot_name}")
    
    cache_section = doc.get(cache_type)
    if not cache_section:
        raise ValueError(f"'{cache_type}' not found in shot '{shot_name}'")

    version_path = cache_section.get(version)
    if not version_path:
        raise ValueError(f"Version '{version}' not found under '{cache_type}' for shot '{shot_name}'")

    return version_path

# 테스트
if __name__ == "__main__":
    path = "AAA_084_0010/cache_usd_geo/v002"
    result = get_usd_path(path)
    print(f"Resolved Path: {result}")
