import os
from typing import TypedDict
from pathlib import Path

from dotenv import load_dotenv
load_dotenv(os.path.dirname(os.path.dirname(__file__)))

from langgraph.graph import StateGraph, END
from langchain_openai import ChatOpenAI

# --- minimal state ---
class State(TypedDict, total=False):
    goal: str
    plan: str
    written: str
    verdict: str

# --- tiny LLM helper (OpenRouter only) ---
def llm():
    return ChatOpenAI(
        model=os.getenv("MODEL_NAME", "openai/gpt-4o"),
        api_key=os.getenv("OPENROUTER_API_KEY"),
        base_url=os.getenv("OPENROUTER_BASE_URL", "https://openrouter.ai/api/v1"),
        temperature=0,
        default_headers={
            "HTTP-Referer": os.getenv("OPENROUTER_APP_URL", ""),
            "X-Title": os.getenv("OPENROUTER_APP_TITLE", "Alis Agent"),
        },
    )

# Per-role LLM helper
def llm_from_env(var_name: str, default: str):
    return ChatOpenAI(
        model=os.getenv(var_name, default),
        api_key=os.getenv("OPENROUTER_API_KEY"),
        base_url=os.getenv("OPENROUTER_BASE_URL", "https://openrouter.ai/api/v1"),
        temperature=0,
        max_tokens=int(os.getenv(
            "ARCH_MAX_TOKENS" if var_name=="ARCH_MODEL" else "BASIC_MAX_TOKENS",
            "2048" if var_name!="ARCH_MODEL" else "4096"
        )),
        default_headers={
            "HTTP-Referer": os.getenv("OPENROUTER_APP_URL",""),
            "X-Title": os.getenv("OPENROUTER_APP_TITLE","Alis Agent"),
        },
    )

# --- nodes ---
def architect_node(state: State) -> State:
    prompt = (
        "You are the Architect for Alis. "
        "Given the goal, produce a minimal 3-step plan with acceptance criteria. "
        "Return plain text bullets.\n\nGoal:\n" + state["goal"]
    )
    plan = llm_from_env("ARCH_MODEL", "anthropic/claude-3.5-sonnet").invoke(prompt).content
    return {"plan": plan}

def executor_node(state: State) -> State:
    # YAGNI: write 3 tiny spec files under /specs and stop
    repo = Path(os.getenv("REPO_ROOT", ".")).resolve()
    specs = (repo / "specs"); specs.mkdir(parents=True, exist_ok=True)
    (specs / "spec.md").write_text("# Spec\n\n" + state["plan"], encoding="utf-8")
    (specs / "tasks.yaml").write_text("tasks:\n  - stub: true\n", encoding="utf-8")
    (specs / "tests.yaml").write_text("tests:\n  - Alis.Smoke\n", encoding="utf-8")
    return {"written": str(specs)}

def tester_node(state: State) -> State:
    # Stubbed tester: always pass. Wire your Windows test bridge later.
    # If you later make the tester ask LLM to summarize logs, use BASIC_MODEL here:
    # llm_from_env("BASIC_MODEL", "xai/grok-2-mini").invoke("Summarize ...")
    return {"verdict": "OK (stubbed)"}

# --- build graph ---
def build():
    g = StateGraph(State)
    g.add_node("architect", architect_node)
    g.add_node("executor", executor_node)
    g.add_node("tester", tester_node)

    g.set_entry_point("architect")
    g.add_edge("architect", "executor")
    g.add_edge("executor", "tester")
    g.add_edge("tester", END)
    return g.compile()

agent = build()
