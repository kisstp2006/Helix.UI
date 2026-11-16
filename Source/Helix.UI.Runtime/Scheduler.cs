using System;
using System.Collections.Generic;

namespace HelixEngine.UI
{
    public struct TimerState
    {
        public long start { get; set; }
        public long now { get; set; }
        public long deltaTime => now - start;
    }

    public interface IVisualElementScheduledItem
    {
        Element element { get; }
        bool isActive { get; }
        void Resume();
        void Pause();
        void ExecuteLater(long delayMs);
        IVisualElementScheduledItem StartingIn(long delayMs);
        IVisualElementScheduledItem Every(long intervalMs);
        IVisualElementScheduledItem Until(Func<bool> stopCondition);
        IVisualElementScheduledItem ForDuration(long durationMs);
    }

    public interface IVisualElementScheduler
    {
        IVisualElementScheduledItem Execute(Action<TimerState> timerUpdateEvent);
        IVisualElementScheduledItem Execute(Action updateEvent);
    }

    // Items of elements run once a frame (the views' tick, in milliseconds), while their element is in a panel.
    internal static class Scheduler
    {
        static readonly List<Item> s_Items = new List<Item>();
        static long s_Now;

        internal static long Now => s_Now;

        internal sealed class Item : IVisualElementScheduledItem
        {
            public Element element { get; set; }
            public bool isActive { get; private set; }
            internal Action<TimerState> action;
            internal long delay, interval = -1, duration = -1, startedAt, lastRun = -1, due;
            internal Func<bool> until;

            public void Resume()
            {
                if (isActive)
                    return;
                isActive = true;
                startedAt = s_Now;
                due = s_Now + delay;
                if (!s_Items.Contains(this))
                    s_Items.Add(this);
            }

            public void Pause() => isActive = false;

            public void ExecuteLater(long delayMs)
            {
                delay = delayMs;
                isActive = false;
                Resume();
            }

            public IVisualElementScheduledItem StartingIn(long delayMs)
            {
                delay = delayMs;
                due = startedAt + delayMs;
                return this;
            }

            public IVisualElementScheduledItem Every(long intervalMs)
            {
                interval = intervalMs;
                return this;
            }

            public IVisualElementScheduledItem Until(Func<bool> stopCondition)
            {
                until = stopCondition;
                return this;
            }

            public IVisualElementScheduledItem ForDuration(long durationMs)
            {
                duration = durationMs;
                return this;
            }
        }

        internal static void Tick(long now)
        {
            s_Now = now;
            foreach (Item item in s_Items.ToArray())
            {
                if (!item.isActive)
                {
                    s_Items.Remove(item);
                    continue;
                }
                // Elements outside of a panel wait.
                if (item.element.panel == null || now < item.due)
                    continue;
                if (item.until != null && item.until())
                {
                    item.Pause();
                    continue;
                }
                long previous = item.lastRun < 0 ? item.startedAt : item.lastRun;
                item.lastRun = now;
                item.action(new TimerState { start = previous, now = now });
                bool expired = item.duration >= 0 && now - item.startedAt >= item.duration;
                if (item.interval < 0 || expired)
                    item.Pause();
                else
                    item.due = now + item.interval;
            }
        }
    }

    public partial class Element
    {
        sealed class ElementScheduler : IVisualElementScheduler
        {
            readonly Element m_Element;

            public ElementScheduler(Element element) { m_Element = element; }

            public IVisualElementScheduledItem Execute(Action<TimerState> timerUpdateEvent)
            {
                var item = new Scheduler.Item { element = m_Element, action = timerUpdateEvent };
                item.Resume();
                return item;
            }

            public IVisualElementScheduledItem Execute(Action updateEvent) => Execute(_ => updateEvent());
        }

        public IVisualElementScheduler schedule => new ElementScheduler(this);
    }
}
